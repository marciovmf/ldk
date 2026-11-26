#include <system/ldk_grass_system.h>

#include <ldk.h>
#include <ldk_image.h>
#include <ldk_terrain.h>
#include <ldk_mesh.h>
#include <ldk_mesh_asset.h>
#include <ldk_material_asset.h>
#include <component/ldk_grass_interact.h>
#include <component/ldk_terrain_component.h>
#include <component/ldk_transform.h>
#include <module/ldk_ecs.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_renderer.h>
#include <module/ldk_scenegraph.h>

#include <float.h>
#include <stdarg.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LDK_GRASS_PI 3.14159265358979323846f
/* Independent chunks for editor-authored terrain vegetation. The mask is
 * sampled per instance; the tile only bounds candidate generation. */
#define LDK_GRASS_TERRAIN_PATCH_CELLS 32u
/* A patch can contain hundreds of thousands of scattered candidates. A
 * density brush must not reevaluate their random/clump positions every dab.
 * Bin all candidates once, keeping the RNG state after placement so a
 * refreshed candidate reproduces the exact same transform. */
#define LDK_GRASS_TERRAIN_CANDIDATE_BINS 32u
#define LDK_GRASS_TERRAIN_CANDIDATE_BIN_COUNT \
  (LDK_GRASS_TERRAIN_CANDIDATE_BINS * LDK_GRASS_TERRAIN_CANDIDATE_BINS)
#define LDK_GRASS_TERRAIN_PATCH_LIMIT 65536u
#define LDK_GRASS_INVALID_BATCH UINT32_MAX
#define LDK_GRASS_RIBBON_VERTEX_COUNT                                        \
  (LDK_GRASS_PROFILE_POINT_COUNT * 2u)
#define LDK_GRASS_THORN_COUNT 6u
#define LDK_GRASS_THORN_VERTEX_COUNT 4u
#define LDK_GRASS_THORN_INDEX_COUNT 9u
#define LDK_GRASS_PLUME_COUNT 3u
#define LDK_GRASS_PLUME_SEGMENT_COUNT 16u
#define LDK_GRASS_PLUMED_VERTEX_COUNT                                      \
  (LDK_GRASS_RIBBON_VERTEX_COUNT +                                        \
      LDK_GRASS_PLUME_COUNT * (LDK_GRASS_PLUME_SEGMENT_COUNT + 1u) * 2u)
#define LDK_GRASS_PLUMED_INDEX_COUNT                                       \
  ((LDK_GRASS_PROFILE_POINT_COUNT - 1u) * 6u +                             \
      LDK_GRASS_PLUME_COUNT * LDK_GRASS_PLUME_SEGMENT_COUNT * 6u)
#define LDK_GRASS_MAX_MESH_VERTEX_COUNT                                      \
  (LDK_GRASS_PROFILE_POINT_COUNT * LDK_GRASS_MAX_SIDE_COUNT +               \
      LDK_GRASS_MAX_SIDE_COUNT + 1u +                                       \
      LDK_GRASS_THORN_COUNT * LDK_GRASS_THORN_VERTEX_COUNT +                \
      LDK_GRASS_PLUMED_VERTEX_COUNT)
#define LDK_GRASS_MAX_MESH_INDEX_COUNT                                       \
  ((LDK_GRASS_PROFILE_POINT_COUNT - 1u) * LDK_GRASS_MAX_SIDE_COUNT * 6u +   \
      LDK_GRASS_MAX_SIDE_COUNT * 3u +                                       \
      LDK_GRASS_THORN_COUNT * LDK_GRASS_THORN_INDEX_COUNT +                 \
      LDK_GRASS_PLUMED_INDEX_COUNT)
#define LDK_GRASS_THORN_MAX_LENGTH_SCALE 0.75f
#define LDK_GRASS_THORNY_BEND_MAX_SCALE 1.25f
#define LDK_GRASS_INTERACTION_RESOLUTION 128u
#define LDK_GRASS_INTERACTION_WORLD_SIZE 32.0f
#define LDK_GRASS_INTERACTION_DEFAULT_RECOVERY_TIME 2.5f
#define LDK_GRASS_INTERACTION_MAX_RECOVERY_TIME 16.0f
#define LDK_GRASS_INTERACTION_EPSILON 0.001f
#define LDK_GRASS_INTERACTION_TELEPORT_DISTANCE \
  (LDK_GRASS_INTERACTION_WORLD_SIZE * 0.5f)


typedef struct LDKGrassResolvedDesc
{
  LDKGrassPlane plane;
  LDKGrassBladeShape shape;
  float density;
  float clumping;
  float clump_radius;
  float min_height;
  float max_height;
  bool use_noise_height;
  float min_tilt;
  float max_tilt;
  float tilt_direction;
  rgba32 bottom_color;
  rgba32 top_color;
  bool use_noise_color;
  bool use_custom_noise_palette;
  u32 noise_color_stop_0_color;
  u32 noise_color_stop_1_color;
  u32 noise_color_stop_2_color;
  u32 noise_color_stop_3_color;
  float specular;
  float shininess;
  float emission;
  float curvature;
  float wind_strength;
  float wind_speed;
  float wind_direction;
  float interaction_recovery_time;
  u32 seed;
  LDKGrassLayerKind kind;
  LDKAssetPath mesh_path;
  u32 mesh_index;
  LDKAssetPath material_paths[LDK_GRASS_MESH_MATERIAL_SLOT_COUNT];
  Vec3 scale_min;
  Vec3 scale_max;
  Vec3 rotation_min;
  Vec3 rotation_max;
  bool align_to_normal;
  bool follow_terrain;
  LDKEntity terrain_entity;
  LDKAssetImage density_mask;
  u64 terrain_vegetation_layer_id;
  bool casts_shadows;
  bool unlit;
} LDKGrassResolvedDesc;

typedef struct LDKGrassTerrainCandidate
{
  Vec2 position;
  u32 random_state;
  u32 next;
} LDKGrassTerrainCandidate;

typedef struct LDKGrassTerrainCandidateIndex
{
  LDKGrassTerrainCandidate *candidates;
  /* Candidate ID -> accepted instance slot; inverse maps dense slots back
   * to stable candidate IDs. These are rebuilt with the candidate scatter and
   * updated in O(touched candidates) for regional density brush strokes. */
  u32 *candidate_slots;
  u32 *slot_candidates;
  u32 count;
  u32 heads[LDK_GRASS_TERRAIN_CANDIDATE_BIN_COUNT];
  bool ready;
} LDKGrassTerrainCandidateIndex;

typedef struct LDKGrassPatchEntry
{
  LDKGrassPatchDesc desc;
  LDKGrassTypeDesc type;
  Mat4 *instances;
  /* World-space terrain normal at each accepted instance. Sculpt can
   * reproject an existing instance without rerunning its random scatter. */
  Vec3 *terrain_normals;
  LDKGrassTerrainCandidateIndex *candidate_index;
  u32 instance_count;
  u32 instance_capacity;
  u32 instance_offset;
  u32 lod_selected_count;
  u32 batch_index;
  u32 version;
  Vec3 bounds_min;
  Vec3 bounds_max;
  bool terrain_owned;
  u32 terrain_tile_x;
  u32 terrain_tile_z;
  bool alive;
} LDKGrassPatchEntry;

typedef struct LDKGrassBatch
{
  LDKGrassBladeShape shape;
  LDKGrassLayerKind kind;
  LDKAssetPath mesh_path;
  u32 mesh_index;
  LDKAssetPath material_paths[LDK_GRASS_MESH_MATERIAL_SLOT_COUNT];
  rgba32 bottom_color;
  rgba32 top_color;
  bool use_noise_color;
  bool use_custom_noise_palette;
  u32 noise_color_stop_0_color;
  u32 noise_color_stop_1_color;
  u32 noise_color_stop_2_color;
  u32 noise_color_stop_3_color;
  float specular;
  float shininess;
  float emission;
  float curvature;
  float wind_strength;
  float wind_speed;
  float wind_direction;
  float interaction_recovery_time;
  bool casts_shadows;
  bool unlit;

  Mat4 *instances;
  u32 *instance_colors;
  u32 instance_count;
  u32 instance_capacity;
  u32 index_count;
  LDKResourceMesh mesh;
  LDKResourceMesh lod_mesh;
  u32 lod_index_count;
  LDKResourceMaterial material;
  /* Mesh-backed layers: one renderer material per authored material slot.
   * The imported mesh is shared by all submesh submissions. */
  LDKResourceMaterial slot_materials[LDK_GRASS_MESH_MATERIAL_SLOT_COUNT];
  LDKResourceTexture slot_textures[LDK_GRASS_MESH_MATERIAL_SLOT_COUNT];
  LDKResourceTexture slot_normal_maps[LDK_GRASS_MESH_MATERIAL_SLOT_COUNT];
  LDKResourceTexture slot_specular_maps[LDK_GRASS_MESH_MATERIAL_SLOT_COUNT];
  LDKMeshSubmesh *submeshes;
  u32 submesh_count;
  u32 material_slot_count;
  float mesh_min_y;
  float mesh_inv_height;
  LDKResourceInstanceSet instance_set;
  LDKResourceInstanceSet lod_instance_set;
  Mat4 *lod_instances;
  u32 *lod_instance_colors;
  u32 lod_instance_count;
  u32 lod_instance_capacity;
  u32 lod_required_count;
  u64 lod_selection_hash;
  u64 lod_frame_hash;
  u32 submit_offset;
  u32 submit_count;
  bool submit_ready;
  bool lod_dirty;
  bool dirty;
} LDKGrassBatch;

typedef struct LDKGrassInteraction
{
  Vec3 from;
  Vec3 to;
  float radius;
  float strength;
} LDKGrassInteraction;

typedef struct LDKGrassInteractionFieldCell
{
  Vec2 bend;
  float age;
} LDKGrassInteractionFieldCell;

typedef struct LDKGrassInteractorState
{
  LDKEntity entity;
  Vec3 previous_position;
  bool initialized;
  bool seen;
} LDKGrassInteractorState;

/* A terrain layer is invalidated independently of other layers/terrains. */
typedef struct LDKGrassTerrainLayerCache
{
  LDKEntity entity;
  u64 layer_id;
  u64 hash;
  u64 heightmap_revision;
  u64 density_revision;
  u64 asset_revision;
  bool seen;
} LDKGrassTerrainLayerCache;

typedef struct LDKGrassRuntime
{
  LDKGrassSystem *owner;
  LDKRenderer *renderer;
  LDKImage *color_noise_image;
  LDKGrassPatchEntry *patches;
  u32 patch_count;
  u32 patch_capacity;
  LDKGrassBatch *batches;
  u32 batch_count;
  u32 batch_capacity;
  LDKGrassTerrainLayerCache *terrain_layers;
  u32 terrain_layer_count;
  u32 terrain_layer_capacity;

  LDKGrassInteraction *interactions;
  u32 interaction_count;
  u32 interaction_capacity;
  LDKGrassInteractorState *interactor_states;
  u32 interactor_state_count;
  u32 interactor_state_capacity;
  LDKGrassInteractionFieldCell *interaction_field;
  Vec2 *interaction_stamps;
  LDKGrassInteractionFieldCell *interaction_scratch;
  u8 *interaction_pixels;
  LDKResourceTexture interaction_texture;
  Vec2 interaction_origin;
  bool interaction_origin_initialized;
  bool interaction_field_active;

  u64 config_hash;
  u64 terrain_sync_hash;
  u64 terrain_quick_hash;
  LDKAssetImage terrain_mask_assets[LDK_TERRAIN_VEGETATION_LAYER_COUNT];
  u64 terrain_mask_revisions[LDK_TERRAIN_VEGETATION_LAYER_COUNT];
  LDKAssetImage terrain_heightmap_asset;
  u64 terrain_heightmap_revision;
  bool initialized;
} LDKGrassRuntime;

static LDKGrassRuntime s_grass;
static bool s_grass_editor_preview_active;

static LDKGrassBladeMeshGenerator
    s_custom_generators[LDK_GRASS_CUSTOM_BLADE_PRESET_COUNT];
static u32 s_custom_generations[LDK_GRASS_CUSTOM_BLADE_PRESET_COUNT];
static bool s_custom_error_reported[LDK_GRASS_CUSTOM_BLADE_PRESET_COUNT];

bool ldk_grass_blade_generator_register(
    LDKGrassBladePreset preset, LDKGrassBladeMeshGenerator generator)
{
  if (preset < LDK_GRASS_BLADE_PRESET_CUSTOM_0 ||
      preset > LDK_GRASS_BLADE_PRESET_CUSTOM_7)
  {
    return false;
  }

  u32 index = (u32)(preset - LDK_GRASS_BLADE_PRESET_CUSTOM_0);
  s_custom_generators[index] = generator;
  s_custom_error_reported[index] = false;
  ++s_custom_generations[index];
  return true;
}


static bool s_allocation_size_is_valid(u32 count, size_t element_size)
{
  return element_size != 0u &&
         (uintmax_t)count <= (uintmax_t)SIZE_MAX / (uintmax_t)element_size;
}

static bool s_vec3_is_finite(Vec3 value)
{
  return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static float s_clamp01(float value)
{
  if (value < 0.0f)
  {
    return 0.0f;
  }
  if (value > 1.0f)
  {
    return 1.0f;
  }
  return value;
}

static bool s_interactions_reserve(u32 needed)
{
  LDKGrassInteraction *new_interactions;
  u32 new_capacity;

  if (needed <= s_grass.interaction_capacity)
  {
    return true;
  }

  new_capacity = s_grass.interaction_capacity == 0u
      ? 16u : s_grass.interaction_capacity;
  while (new_capacity < needed)
  {
    if (new_capacity > UINT32_MAX / 2u)
    {
      new_capacity = needed;
      break;
    }
    new_capacity *= 2u;
  }

  if (!s_allocation_size_is_valid(
          new_capacity, sizeof(LDKGrassInteraction)))
  {
    return false;
  }

  new_interactions = realloc(s_grass.interactions,
      (size_t)new_capacity * sizeof(LDKGrassInteraction));
  if (!new_interactions)
  {
    return false;
  }

  s_grass.interactions = new_interactions;
  s_grass.interaction_capacity = new_capacity;
  return true;
}

static bool s_interaction_queue(Vec3 from, Vec3 to,
    float radius, float strength)
{
  LDKGrassInteraction *interaction;

  if (!s_grass.initialized || !s_vec3_is_finite(from) ||
      !s_vec3_is_finite(to) || !isfinite(radius) || radius <= 0.0f ||
      !isfinite(strength) || strength <= 0.0f ||
      !s_interactions_reserve(s_grass.interaction_count + 1u))
  {
    return false;
  }

  interaction = &s_grass.interactions[s_grass.interaction_count++];
  interaction->from = from;
  interaction->to = to;
  interaction->radius = radius;
  interaction->strength = strength;
  return true;
}

static bool s_interactor_states_reserve(u32 needed)
{
  LDKGrassInteractorState *new_states;
  u32 new_capacity;

  if (needed <= s_grass.interactor_state_capacity)
  {
    return true;
  }

  new_capacity = s_grass.interactor_state_capacity == 0u
      ? 16u : s_grass.interactor_state_capacity;
  while (new_capacity < needed)
  {
    if (new_capacity > UINT32_MAX / 2u)
    {
      new_capacity = needed;
      break;
    }
    new_capacity *= 2u;
  }

  if (!s_allocation_size_is_valid(
          new_capacity, sizeof(LDKGrassInteractorState)))
  {
    return false;
  }

  new_states = realloc(s_grass.interactor_states,
      (size_t)new_capacity * sizeof(LDKGrassInteractorState));
  if (!new_states)
  {
    return false;
  }

  s_grass.interactor_states = new_states;
  s_grass.interactor_state_capacity = new_capacity;
  return true;
}

static bool s_entity_equal(LDKEntity a, LDKEntity b)
{
  return a.index == b.index && a.version == b.version;
}

static LDKGrassInteractorState *s_interactor_state_get(LDKEntity entity)
{
  for (u32 i = 0u; i < s_grass.interactor_state_count; ++i)
  {
    LDKGrassInteractorState *state = &s_grass.interactor_states[i];
    if (s_entity_equal(state->entity, entity))
    {
      return state;
    }
  }

  if (!s_interactor_states_reserve(s_grass.interactor_state_count + 1u))
  {
    return NULL;
  }

  LDKGrassInteractorState *state =
      &s_grass.interactor_states[s_grass.interactor_state_count++];
  memset(state, 0, sizeof(*state));
  state->entity = entity;
  return state;
}

static void s_interactor_states_begin_frame(void)
{
  for (u32 i = 0u; i < s_grass.interactor_state_count; ++i)
  {
    s_grass.interactor_states[i].seen = false;
  }
}

static void s_interactor_states_end_frame(void)
{
  u32 write_index = 0u;

  for (u32 i = 0u; i < s_grass.interactor_state_count; ++i)
  {
    if (!s_grass.interactor_states[i].seen)
    {
      continue;
    }
    if (write_index != i)
    {
      s_grass.interactor_states[write_index] = s_grass.interactor_states[i];
    }
    write_index += 1u;
  }
  s_grass.interactor_state_count = write_index;
}

static void s_interaction_components_collect(void)
{
  LDKComponentRegistry *components = ldk_ecs_component_registry_get();
  XArray *interactors;
  XArray *owners;

  s_interactor_states_begin_frame();
  if (!components)
  {
    s_interactor_states_end_frame();
    return;
  }

  interactors = ldk_component_store_get(
      components, LDK_COMPONENT_TYPE_GRASS_INTERACT);
  owners = ldk_component_owners_get(
      components, LDK_COMPONENT_TYPE_GRASS_INTERACT);
  if (!interactors || !owners)
  {
    s_interactor_states_end_frame();
    return;
  }

  for (u32 i = 0u; i < x_array_count(interactors); ++i)
  {
    LDKGrassInteractComponent *interactor = x_array_get(interactors, i);
    LDKEntity *entity = x_array_get(owners, i);
    LDKGrassInteractorState *state;
    Mat4 world;
    Vec3 position;
    Vec3 previous;
    Vec2 movement;

    if (!interactor || !entity)
    {
      continue;
    }

    state = s_interactor_state_get(*entity);
    if (!state)
    {
      continue;
    }
    state->seen = true;

    if (!interactor->enabled ||
        !ldk_ecs_component_is_enabled(
            *entity, LDK_COMPONENT_TYPE_GRASS_INTERACT))
    {
      state->initialized = false;
      continue;
    }

    if (!isfinite(interactor->radius) || interactor->radius <= 0.0f ||
        !isfinite(interactor->strength) || interactor->strength <= 0.0f ||
        !ldk_scenegraph_update_entity(*entity) ||
        !ldk_transform_get_world_matrix(*entity, &world))
    {
      state->initialized = false;
      continue;
    }

    position = vec3_make(world.m[12], world.m[13], world.m[14]);
    if (!s_vec3_is_finite(position))
    {
      state->initialized = false;
      continue;
    }

    previous = state->initialized ? state->previous_position : position;
    movement = vec2_make(position.x - previous.x, position.z - previous.z);
    if (vec2_len2(movement) >
        LDK_GRASS_INTERACTION_TELEPORT_DISTANCE *
            LDK_GRASS_INTERACTION_TELEPORT_DISTANCE)
    {
      previous = position;
    }

    (void)s_interaction_queue(
        previous, position, interactor->radius, interactor->strength);

    state->previous_position = position;
    state->initialized = true;
  }

  s_interactor_states_end_frame();
}

static void s_interaction_field_cells_clear(
    LDKGrassInteractionFieldCell *cells, u32 count)
{
  if (!cells)
  {
    return;
  }

  for (u32 i = 0u; i < count; ++i)
  {
    cells[i].bend = vec2_make(0.0f, 0.0f);
    cells[i].age = LDK_GRASS_INTERACTION_MAX_RECOVERY_TIME;
  }
}

static bool s_interaction_field_allocate(void)
{
  const u32 cell_count = LDK_GRASS_INTERACTION_RESOLUTION *
      LDK_GRASS_INTERACTION_RESOLUTION;
  const u32 pixel_count = cell_count * 4u;

  if (!s_allocation_size_is_valid(
          cell_count, sizeof(LDKGrassInteractionFieldCell)) ||
      !s_allocation_size_is_valid(cell_count, sizeof(Vec2)) ||
      !s_allocation_size_is_valid(pixel_count, sizeof(u8)))
  {
    return false;
  }

  s_grass.interaction_field =
      malloc((size_t)cell_count * sizeof(LDKGrassInteractionFieldCell));
  s_grass.interaction_stamps = calloc(cell_count, sizeof(Vec2));
  s_grass.interaction_scratch =
      malloc((size_t)cell_count * sizeof(LDKGrassInteractionFieldCell));
  s_grass.interaction_pixels = malloc(pixel_count);
  if (!s_grass.interaction_field || !s_grass.interaction_stamps ||
      !s_grass.interaction_scratch || !s_grass.interaction_pixels)
  {
    return false;
  }

  s_interaction_field_cells_clear(s_grass.interaction_field, cell_count);
  s_interaction_field_cells_clear(s_grass.interaction_scratch, cell_count);
  for (u32 i = 0u; i < cell_count; ++i)
  {
    s_grass.interaction_pixels[i * 4u] = 128u;
    s_grass.interaction_pixels[i * 4u + 1u] = 128u;
    s_grass.interaction_pixels[i * 4u + 2u] = 255u;
    s_grass.interaction_pixels[i * 4u + 3u] = 255u;
  }
  return true;
}

static bool s_interaction_texture_create(void)
{
  LDKRendererTextureOptions options;
  LDKRendererTextureDesc desc = {0};
  const u64 byte_count = (u64)LDK_GRASS_INTERACTION_RESOLUTION *
      (u64)LDK_GRASS_INTERACTION_RESOLUTION * 4u;

  if (!s_grass.renderer || !s_grass.interaction_pixels)
  {
    return false;
  }

  ldk_renderer_texture_options_defaults(&options);
  options.generate_mipmaps = false;
  options.min_filter = LDK_RHI_FILTER_LINEAR;
  options.mag_filter = LDK_RHI_FILTER_LINEAR;
  options.mip_filter = LDK_RHI_FILTER_NONE;
  options.wrap_u = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  options.wrap_v = LDK_RHI_WRAP_CLAMP_TO_EDGE;
  options.wrap_w = LDK_RHI_WRAP_CLAMP_TO_EDGE;

  desc.width = LDK_GRASS_INTERACTION_RESOLUTION;
  desc.height = LDK_GRASS_INTERACTION_RESOLUTION;
  desc.channel_count = 4u;
  desc.pixels = s_grass.interaction_pixels;
  desc.byte_count = byte_count;
  desc.options = &options;

  s_grass.interaction_texture =
      ldk_renderer_texture_create(s_grass.renderer, &desc);
  if (!ldk_renderer_texture_is_valid(
          s_grass.renderer, s_grass.interaction_texture))
  {
    s_grass.interaction_texture = ldk_renderer_texture_null();
    return false;
  }

  s_grass.interaction_origin = vec2_make(0.0f, 0.0f);
  return ldk_renderer_vegetation_interaction_set(s_grass.renderer,
      s_grass.interaction_texture, s_grass.interaction_origin,
      LDK_GRASS_INTERACTION_WORLD_SIZE);
}

static void s_interaction_field_release(void)
{
  if (s_grass.renderer)
  {
    ldk_renderer_vegetation_interaction_clear(s_grass.renderer);
    if (ldk_renderer_texture_is_valid(
            s_grass.renderer, s_grass.interaction_texture))
    {
      ldk_renderer_texture_destroy(
          s_grass.renderer, s_grass.interaction_texture);
    }
  }

  free(s_grass.interactions);
  free(s_grass.interactor_states);
  free(s_grass.interaction_field);
  free(s_grass.interaction_stamps);
  free(s_grass.interaction_scratch);
  free(s_grass.interaction_pixels);
  s_grass.interactions = NULL;
  s_grass.interactor_states = NULL;
  s_grass.interactor_state_count = 0u;
  s_grass.interactor_state_capacity = 0u;
  s_grass.interaction_field = NULL;
  s_grass.interaction_stamps = NULL;
  s_grass.interaction_scratch = NULL;
  s_grass.interaction_pixels = NULL;
  s_grass.interaction_count = 0u;
  s_grass.interaction_capacity = 0u;
  s_grass.interaction_texture = ldk_renderer_texture_null();
  s_grass.interaction_origin_initialized = false;
  s_grass.interaction_field_active = false;
}

static void s_interaction_field_shift(Vec2 desired_origin)
{
  const i32 resolution = (i32)LDK_GRASS_INTERACTION_RESOLUTION;
  const float texel_size = LDK_GRASS_INTERACTION_WORLD_SIZE /
      (float)LDK_GRASS_INTERACTION_RESOLUTION;
  i32 offset_x;
  i32 offset_z;

  if (!s_grass.interaction_origin_initialized)
  {
    s_grass.interaction_origin = desired_origin;
    s_grass.interaction_origin_initialized = true;
    return;
  }

  offset_x = (i32)lroundf(
      (desired_origin.x - s_grass.interaction_origin.x) / texel_size);
  offset_z = (i32)lroundf(
      (desired_origin.y - s_grass.interaction_origin.y) / texel_size);
  if (offset_x == 0 && offset_z == 0)
  {
    return;
  }

  s_interaction_field_cells_clear(s_grass.interaction_scratch,
      LDK_GRASS_INTERACTION_RESOLUTION *
          LDK_GRASS_INTERACTION_RESOLUTION);

  if (abs(offset_x) < resolution && abs(offset_z) < resolution)
  {
    for (i32 z = 0; z < resolution; ++z)
    {
      i32 old_z = z + offset_z;
      if (old_z < 0 || old_z >= resolution)
      {
        continue;
      }
      for (i32 x = 0; x < resolution; ++x)
      {
        i32 old_x = x + offset_x;
        if (old_x < 0 || old_x >= resolution)
        {
          continue;
        }
        s_grass.interaction_scratch[z * resolution + x] =
            s_grass.interaction_field[old_z * resolution + old_x];
      }
    }
  }

  {
    LDKGrassInteractionFieldCell *tmp = s_grass.interaction_field;
    s_grass.interaction_field = s_grass.interaction_scratch;
    s_grass.interaction_scratch = tmp;
  }
  s_grass.interaction_origin = desired_origin;
}

static void s_interaction_field_recenter(void)
{
  const float texel_size = LDK_GRASS_INTERACTION_WORLD_SIZE /
      (float)LDK_GRASS_INTERACTION_RESOLUTION;
  float min_x = FLT_MAX;
  float min_z = FLT_MAX;
  float max_x = -FLT_MAX;
  float max_z = -FLT_MAX;

  if (s_grass.interaction_count == 0u)
  {
    return;
  }

  for (u32 i = 0u; i < s_grass.interaction_count; ++i)
  {
    const LDKGrassInteraction *interaction = &s_grass.interactions[i];
    min_x = fminf(min_x, fminf(interaction->from.x, interaction->to.x));
    min_z = fminf(min_z, fminf(interaction->from.z, interaction->to.z));
    max_x = fmaxf(max_x, fmaxf(interaction->from.x, interaction->to.x));
    max_z = fmaxf(max_z, fmaxf(interaction->from.z, interaction->to.z));
  }

  {
    Vec2 center = vec2_make((min_x + max_x) * 0.5f,
        (min_z + max_z) * 0.5f);
    Vec2 desired = vec2_make(
        floorf((center.x - LDK_GRASS_INTERACTION_WORLD_SIZE * 0.5f) /
                   texel_size) * texel_size,
        floorf((center.y - LDK_GRASS_INTERACTION_WORLD_SIZE * 0.5f) /
                   texel_size) * texel_size);
    s_interaction_field_shift(desired);
  }
}

static Vec2 s_interaction_stamp_direction(
    const LDKGrassInteraction *interaction, Vec2 sample, Vec2 closest,
    Vec2 current)
{
  Vec2 radial = vec2_sub(sample, current);

  /*
   * The current footprint is always radial around the interactor, regardless
   * of whether it moved this frame. This keeps moving and stationary states
   * visually identical around the entity. The rest of the capsule trail bends
   * away from the closest point on the travelled segment.
   */
  if (vec2_len2(radial) <=
      interaction->radius * interaction->radius &&
      vec2_len2(radial) > STDXM_EPS * STDXM_EPS)
  {
    return vec2_norm(radial);
  }

  radial = vec2_sub(sample, closest);
  if (vec2_len2(radial) <= STDXM_EPS * STDXM_EPS)
  {
    return vec2_make(0.0f, 0.0f);
  }
  return vec2_norm(radial);
}

static void s_interaction_stamp(const LDKGrassInteraction *interaction)
{
  const i32 resolution = (i32)LDK_GRASS_INTERACTION_RESOLUTION;
  const float texel_size = LDK_GRASS_INTERACTION_WORLD_SIZE /
      (float)LDK_GRASS_INTERACTION_RESOLUTION;
  Vec2 a = vec2_make(interaction->from.x, interaction->from.z);
  Vec2 b = vec2_make(interaction->to.x, interaction->to.z);
  Vec2 movement = vec2_sub(b, a);
  float movement_len2 = vec2_len2(movement);
  float inv_segment_len2 = movement_len2 > STDXM_EPS * STDXM_EPS
      ? 1.0f / movement_len2 : 0.0f;
  float min_x = fminf(a.x, b.x) - interaction->radius;
  float min_z = fminf(a.y, b.y) - interaction->radius;
  float max_x = fmaxf(a.x, b.x) + interaction->radius;
  float max_z = fmaxf(a.y, b.y) + interaction->radius;
  i32 first_x = (i32)floorf(
      (min_x - s_grass.interaction_origin.x) / texel_size);
  i32 first_z = (i32)floorf(
      (min_z - s_grass.interaction_origin.y) / texel_size);
  i32 last_x = (i32)floorf(
      (max_x - s_grass.interaction_origin.x) / texel_size);
  i32 last_z = (i32)floorf(
      (max_z - s_grass.interaction_origin.y) / texel_size);

  if (last_x < 0 || last_z < 0 || first_x >= resolution ||
      first_z >= resolution)
  {
    return;
  }
  if (first_x < 0)
  {
    first_x = 0;
  }
  if (first_z < 0)
  {
    first_z = 0;
  }
  if (last_x >= resolution)
  {
    last_x = resolution - 1;
  }
  if (last_z >= resolution)
  {
    last_z = resolution - 1;
  }

  for (i32 z = first_z; z <= last_z; ++z)
  {
    for (i32 x = first_x; x <= last_x; ++x)
    {
      Vec2 sample = vec2_make(
          s_grass.interaction_origin.x + ((float)x + 0.5f) * texel_size,
          s_grass.interaction_origin.y + ((float)z + 0.5f) * texel_size);
      Vec2 closest = a;
      float t = 0.0f;
      Vec2 delta;
      float distance;
      float weight;
      Vec2 direction;
      Vec2 contribution;
      Vec2 stamp;
      float stamp_len2;
      u32 index = (u32)z * LDK_GRASS_INTERACTION_RESOLUTION + (u32)x;

      if (movement_len2 > STDXM_EPS * STDXM_EPS)
      {
        t = s_clamp01(vec2_dot(vec2_sub(sample, a), movement) *
            inv_segment_len2);
        closest = vec2_add(a, vec2_mul(movement, t));
      }

      delta = vec2_sub(sample, closest);
      distance = vec2_len(delta);
      if (distance > interaction->radius)
      {
        continue;
      }

      weight = 1.0f - distance / interaction->radius;
      weight = weight * weight * (3.0f - 2.0f * weight);
      direction = s_interaction_stamp_direction(
          interaction, sample, closest, b);
      contribution = vec2_mul(direction, interaction->strength * weight);
      stamp = vec2_add(s_grass.interaction_stamps[index], contribution);
      stamp_len2 = vec2_len2(stamp);
      if (stamp_len2 > 1.0f)
      {
        stamp = vec2_mul(stamp, 1.0f / sqrtf(stamp_len2));
      }
      s_grass.interaction_stamps[index] = stamp;
    }
  }
}

static u8 s_interaction_encode(float value)
{
  float clamped = value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
  long encoded = lroundf(clamped * 127.0f + 128.0f);
  if (encoded < 0)
  {
    encoded = 0;
  }
  if (encoded > 255)
  {
    encoded = 255;
  }
  return (u8)encoded;
}

static u8 s_interaction_age_encode(float age)
{
  float normalized = s_clamp01(
      age / LDK_GRASS_INTERACTION_MAX_RECOVERY_TIME);
  long encoded = lroundf(normalized * 255.0f);

  if (encoded < 0)
  {
    encoded = 0;
  }
  if (encoded > 255)
  {
    encoded = 255;
  }
  return (u8)encoded;
}

static bool s_interaction_field_update(float dt)
{
  const u32 cell_count = LDK_GRASS_INTERACTION_RESOLUTION *
      LDK_GRASS_INTERACTION_RESOLUTION;
  const u64 byte_count = (u64)cell_count * 4u;
  bool field_active = false;

  if (!s_grass.interaction_field || !s_grass.interaction_stamps ||
      !s_grass.interaction_pixels || !s_grass.renderer)
  {
    return false;
  }

  if (s_grass.interaction_count == 0u &&
      (!s_grass.interaction_field_active || dt == 0.0f))
  {
    return ldk_renderer_vegetation_interaction_set(s_grass.renderer,
        s_grass.interaction_texture, s_grass.interaction_origin,
        LDK_GRASS_INTERACTION_WORLD_SIZE);
  }

  s_interaction_field_recenter();
  memset(s_grass.interaction_stamps, 0, (size_t)cell_count * sizeof(Vec2));
  for (u32 i = 0u; i < s_grass.interaction_count; ++i)
  {
    s_interaction_stamp(&s_grass.interactions[i]);
  }

  for (u32 i = 0u; i < cell_count; ++i)
  {
    LDKGrassInteractionFieldCell *cell = &s_grass.interaction_field[i];
    Vec2 stamp = s_grass.interaction_stamps[i];

    if (vec2_len2(stamp) >
        LDK_GRASS_INTERACTION_EPSILON * LDK_GRASS_INTERACTION_EPSILON)
    {
      cell->bend = stamp;
      cell->age = 0.0f;
    }
    else if (vec2_len2(cell->bend) >
        LDK_GRASS_INTERACTION_EPSILON * LDK_GRASS_INTERACTION_EPSILON)
    {
      cell->age += dt;
      if (cell->age >= LDK_GRASS_INTERACTION_MAX_RECOVERY_TIME)
      {
        cell->bend = vec2_make(0.0f, 0.0f);
        cell->age = LDK_GRASS_INTERACTION_MAX_RECOVERY_TIME;
      }
    }
    else
    {
      cell->bend = vec2_make(0.0f, 0.0f);
      cell->age = LDK_GRASS_INTERACTION_MAX_RECOVERY_TIME;
    }

    if (vec2_len2(cell->bend) >
        LDK_GRASS_INTERACTION_EPSILON * LDK_GRASS_INTERACTION_EPSILON)
    {
      field_active = true;
    }

    s_grass.interaction_pixels[i * 4u] = s_interaction_encode(cell->bend.x);
    s_grass.interaction_pixels[i * 4u + 1u] =
        s_interaction_encode(cell->bend.y);
    s_grass.interaction_pixels[i * 4u + 2u] =
        s_interaction_age_encode(cell->age);
    s_grass.interaction_pixels[i * 4u + 3u] = 255u;
  }

  if (!ldk_renderer_texture_update(s_grass.renderer,
          s_grass.interaction_texture, s_grass.interaction_pixels,
          byte_count) ||
      !ldk_renderer_vegetation_interaction_set(s_grass.renderer,
          s_grass.interaction_texture, s_grass.interaction_origin,
          LDK_GRASS_INTERACTION_WORLD_SIZE))
  {
    return false;
  }

  s_grass.interaction_field_active = field_active;
  s_grass.interaction_count = 0u;
  return true;
}

static float s_shininess_resolve(float shininess)
{
  return shininess == 0.0f ? 32.0f : shininess;
}

static bool s_blade_preset_is_valid(LDKGrassBladePreset preset)
{
  return preset >= LDK_GRASS_BLADE_PRESET_THIN &&
         preset <= LDK_GRASS_BLADE_PRESET_CUSTOM_7;
}

static bool s_type_desc_is_valid(const LDKGrassTypeDesc *desc)
{
  if (!desc || x_smallstr_length(&desc->name) == 0u ||
      !s_blade_preset_is_valid(desc->blade_preset)) return false;
  if (desc->kind > LDK_GRASS_LAYER_TREE) return false;
  if (desc->kind != LDK_GRASS_LAYER_VEGETATION)
  {
    if (desc->mesh_path.length > LDK_ASSET_PATH_MAX_LENGTH ||
        desc->mesh_path.buf[desc->mesh_path.length] != '\0' ||
        !s_vec3_is_finite(desc->scale_min) ||
        !s_vec3_is_finite(desc->scale_max) ||
        !s_vec3_is_finite(desc->rotation_min) ||
        !s_vec3_is_finite(desc->rotation_max)) return false;
    for (u32 slot = 0u; slot < LDK_GRASS_MESH_MATERIAL_SLOT_COUNT;
        ++slot)
      if (desc->material_paths[slot].length > LDK_ASSET_PATH_MAX_LENGTH ||
          desc->material_paths[slot].buf[
              desc->material_paths[slot].length] != '\0') return false;
    const float *lo = &desc->scale_min.x, *hi = &desc->scale_max.x;
    const float *rlo = &desc->rotation_min.x, *rhi = &desc->rotation_max.x;
    for (u32 axis = 0u; axis < 3u; ++axis)
      if (lo[axis] <= 0.0f || hi[axis] < lo[axis] ||
          rhi[axis] < rlo[axis]) return false;
  }
  return
         isfinite(desc->density) && desc->density >= 0.0f &&
         isfinite(desc->clumping) && desc->clumping >= 0.0f &&
         desc->clumping <= 1.0f &&
         isfinite(desc->clump_radius) && desc->clump_radius >= 0.0f &&
         isfinite(desc->min_height) && desc->min_height > 0.0f &&
         isfinite(desc->max_height) && desc->max_height >= desc->min_height &&
         isfinite(desc->blade_width) && desc->blade_width > 0.0f &&
         isfinite(desc->curvature) && desc->curvature >= 0.0f &&
         isfinite(desc->wind_strength) && desc->wind_strength >= 0.0f &&
         isfinite(desc->interaction_recovery_time) &&
         desc->interaction_recovery_time >= 0.0f &&
         desc->interaction_recovery_time <=
             LDK_GRASS_INTERACTION_MAX_RECOVERY_TIME;
}

static bool s_system_config_is_valid(const LDKGrassSystem *system)
{
  if (!system || !isfinite(system->min_tilt_degrees) ||
      system->min_tilt_degrees < 0.0f ||
      !isfinite(system->max_tilt_degrees) ||
      system->max_tilt_degrees < system->min_tilt_degrees ||
      system->max_tilt_degrees >= 90.0f ||
      !isfinite(system->tilt_direction_degrees) ||
      !isfinite(system->wind_direction_degrees) ||
      !isfinite(system->wind_force) || system->wind_force < 0.0f ||
      !isfinite(system->wind_speed) || system->wind_speed < 0.0f ||
      (!x_handle_is_null(system->color_noise_texture.h) &&
          (!isfinite(system->color_noise_scale) ||
              system->color_noise_scale <= 0.0f)) ||
      (!x_handle_is_null(system->color_noise_texture.h) &&
          (!isfinite(system->color_stop_1_position) ||
              !isfinite(system->color_stop_2_position))) ||
      (!x_handle_is_null(system->color_noise_texture.h) &&
          (system->color_stop_1_position < 0.0f ||
              system->color_stop_2_position > 1.0f ||
              (system->color_stop_1_position >=
                      system->color_stop_2_position &&
                      !(system->color_stop_1_position == 0.0f &&
                          system->color_stop_2_position == 0.0f)))) ||
      !isfinite(system->specular) || system->specular < 0.0f ||
      !isfinite(system->shininess) || system->shininess < 0.0f ||
      !isfinite(system->emission) || system->emission < 0.0f)
  {
    return false;
  }


  return true;
}

static bool s_shape_from_type(
    const LDKGrassTypeDesc *type, LDKGrassBladeShape *out_shape)
{
  float width;

  if (!s_type_desc_is_valid(type) || !out_shape)
  {
    return false;
  }

  memset(out_shape, 0, sizeof(*out_shape));
  width = type->blade_width;
  out_shape->side_count = 4u;

  switch (type->blade_preset)
  {
  case LDK_GRASS_BLADE_PRESET_CUSTOM_0:
  case LDK_GRASS_BLADE_PRESET_CUSTOM_1:
  case LDK_GRASS_BLADE_PRESET_CUSTOM_2:
  case LDK_GRASS_BLADE_PRESET_CUSTOM_3:
  case LDK_GRASS_BLADE_PRESET_CUSTOM_4:
  case LDK_GRASS_BLADE_PRESET_CUSTOM_5:
  case LDK_GRASS_BLADE_PRESET_CUSTOM_6:
  case LDK_GRASS_BLADE_PRESET_CUSTOM_7:
    out_shape->topology = LDK_GRASS_BLADE_TOPOLOGY_CUSTOM;
    out_shape->custom_slot =
        (u32)(type->blade_preset - LDK_GRASS_BLADE_PRESET_CUSTOM_0);
    break;
  case LDK_GRASS_BLADE_PRESET_THIN:
    out_shape->topology = LDK_GRASS_BLADE_TOPOLOGY_RIBBON;
    break;
  case LDK_GRASS_BLADE_PRESET_CROSSED:
    out_shape->topology = LDK_GRASS_BLADE_TOPOLOGY_CROSSED_RIBBONS;
    break;
  case LDK_GRASS_BLADE_PRESET_TUFT:
    out_shape->topology = LDK_GRASS_BLADE_TOPOLOGY_TRIPLE_RIBBONS;
    break;
  case LDK_GRASS_BLADE_PRESET_FLAT_TOP:
    out_shape->topology = LDK_GRASS_BLADE_TOPOLOGY_RIBBON;
    break;
  case LDK_GRASS_BLADE_PRESET_THORNY:
    out_shape->topology = LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM;
    break;
  case LDK_GRASS_BLADE_PRESET_PLUMED_STEM:
    out_shape->topology = LDK_GRASS_BLADE_TOPOLOGY_PLUMED_STEM;
    break;
  default:
    return false;
  }

  out_shape->profile[0] = (LDKGrassProfilePoint){0.0f, width};
  if (type->blade_preset == LDK_GRASS_BLADE_PRESET_FLAT_TOP)
  {
    out_shape->profile[1] = (LDKGrassProfilePoint){0.55f, width * 0.95f};
    out_shape->profile[2] = (LDKGrassProfilePoint){0.85f, width * 0.88f};
    out_shape->profile[3] = (LDKGrassProfilePoint){1.0f, width * 0.82f};
  }
  else if (type->blade_preset == LDK_GRASS_BLADE_PRESET_THORNY)
  {
    out_shape->profile[1] = (LDKGrassProfilePoint){0.42f, width * 0.92f};
    out_shape->profile[2] = (LDKGrassProfilePoint){0.76f, width * 0.62f};
    out_shape->profile[3] = (LDKGrassProfilePoint){1.0f, 0.0f};
  }
  else
  {
    out_shape->profile[1] = (LDKGrassProfilePoint){0.55f, width * 0.85f};
    out_shape->profile[2] = (LDKGrassProfilePoint){0.85f, width * 0.50f};
    out_shape->profile[3] = (LDKGrassProfilePoint){1.0f, 0.0f};
  }
  return true;
}

static bool s_resolved_desc_build(const LDKGrassSystem *system,
    const LDKGrassTypeDesc *type, const LDKGrassPatchDesc *patch,
    LDKGrassResolvedDesc *out_desc)
{
  if (!system || !type || !patch || !out_desc ||
      !s_vec3_is_finite(patch->plane.origin) ||
      !s_vec3_is_finite(patch->plane.axis_u) ||
      !s_vec3_is_finite(patch->plane.axis_v) ||
      !isfinite(patch->density_scale) || patch->density_scale < 0.0f)
  {
    return false;
  }
  if (!s_type_desc_is_valid(type)) return false;

  memset(out_desc, 0, sizeof(*out_desc));
  out_desc->plane = patch->plane;
  if (!s_shape_from_type(type, &out_desc->shape))
  {
    return false;
  }
  out_desc->kind = type->kind;
  out_desc->mesh_path = type->mesh_path;
  out_desc->mesh_index = type->mesh_index;
  memcpy(out_desc->material_paths, type->material_paths,
      sizeof(out_desc->material_paths));
  out_desc->scale_min = type->scale_min;
  out_desc->scale_max = type->scale_max;
  out_desc->rotation_min = type->rotation_min;
  out_desc->rotation_max = type->rotation_max;
  out_desc->align_to_normal = type->align_to_normal;
  out_desc->follow_terrain = type->follow_terrain;
  out_desc->density = type->density * patch->density_scale;
  out_desc->clumping = type->clumping;
  out_desc->clump_radius = type->clump_radius;
  out_desc->min_height = type->min_height;
  out_desc->max_height = type->max_height;
  out_desc->use_noise_height =
      type->use_noise_height && s_grass.color_noise_image != NULL;
  out_desc->min_tilt = type->kind != LDK_GRASS_LAYER_VEGETATION ? 0.0f :
      deg_to_rad(system->min_tilt_degrees);
  out_desc->max_tilt = type->kind != LDK_GRASS_LAYER_VEGETATION ? 0.0f :
      deg_to_rad(system->max_tilt_degrees);
  out_desc->tilt_direction = deg_to_rad(system->tilt_direction_degrees);
  out_desc->use_noise_color =
      type->use_noise_color && s_grass.color_noise_image != NULL;
  out_desc->use_custom_noise_palette = type->use_custom_noise_palette;
  out_desc->noise_color_stop_0_color = type->use_custom_noise_palette
      ? type->noise_color_stop_0_color : system->color_stop_0_color;
  out_desc->noise_color_stop_1_color = type->use_custom_noise_palette
      ? type->noise_color_stop_1_color : system->color_stop_1_color;
  out_desc->noise_color_stop_2_color = type->use_custom_noise_palette
      ? type->noise_color_stop_2_color : system->color_stop_2_color;
  out_desc->noise_color_stop_3_color = type->use_custom_noise_palette
      ? type->noise_color_stop_3_color : system->color_stop_3_color;

  out_desc->bottom_color = out_desc->use_noise_color
      ? 0xffffffffu : type->bottom_color;
  out_desc->top_color = out_desc->use_noise_color
      ? 0xffffffffu : type->top_color;
  out_desc->specular = system->specular;
  out_desc->shininess = system->shininess;
  out_desc->emission = system->emission;
  out_desc->curvature = type->kind != LDK_GRASS_LAYER_VEGETATION ? 0.0f : type->curvature;
  out_desc->wind_strength = type->kind == LDK_GRASS_LAYER_DECORATION
      ? 0.0f : type->wind_strength * system->wind_force;
  out_desc->wind_speed = system->wind_speed;
  out_desc->wind_direction = deg_to_rad(system->wind_direction_degrees);
  out_desc->interaction_recovery_time = type->interaction_recovery_time > 0.0f
      ? type->interaction_recovery_time
      : LDK_GRASS_INTERACTION_DEFAULT_RECOVERY_TIME;
  out_desc->seed = patch->seed;
  out_desc->terrain_entity = patch->terrain_entity;
  out_desc->density_mask = patch->density_mask;
  out_desc->terrain_vegetation_layer_id = patch->terrain_vegetation_layer_id;
  out_desc->casts_shadows = system->casts_shadows;
  out_desc->unlit = system->unlit;
  return isfinite(out_desc->density) && isfinite(out_desc->wind_strength);
}

static u32 s_random_next(u32 *state)
{
  u32 value = *state;

  value ^= value << 13u;
  value ^= value >> 17u;
  value ^= value << 5u;
  *state = value ? value : 0x6d2b79f5u;
  return *state;
}

static float s_random_unit(u32 *state)
{
  return (float)(s_random_next(state) >> 8u) / 16777215.0f;
}

static u32 s_random_state(u32 seed)
{
  u32 value = seed + 0x9e3779b9u;

  value ^= value >> 16u;
  value *= 0x7feb352du;
  value ^= value >> 15u;
  value *= 0x846ca68bu;
  value ^= value >> 16u;
  return value ? value : 0x6d2b79f5u;
}

/* Concentrate grass in world-space patches, independent of the number
 * of blades belonging to an individual LDKGrassPatch. The normalized
 * Gaussian kernels preserve the average density across a large area. */
static float s_clumping_density_at(float x, float z)
{
  const float cell_size = 3.0f;
  const float sigma = 0.65f;
  const float inv_sigma2 = 1.0f / (sigma * sigma);
  const float amplitude =
      cell_size * cell_size / (2.0f * LDK_GRASS_PI * sigma * sigma);
  int cell_x = (int)floorf(x / cell_size);
  int cell_z = (int)floorf(z / cell_size);
  float weight = 0.0f;

  for (int dz = -2; dz <= 2; ++dz)
  {
    for (int dx = -2; dx <= 2; ++dx)
    {
      int ix = cell_x + dx;
      int iz = cell_z + dz;
      u32 state = s_random_state(
          (u32)ix * 0x9e3779b9u ^ (u32)iz * 0x85ebca6bu ^ 0x73d45b91u);
      float cx = ((float)ix + 0.15f + 0.70f * s_random_unit(&state)) * cell_size;
      float cz = ((float)iz + 0.15f + 0.70f * s_random_unit(&state)) * cell_size;
      float rx = x - cx;
      float rz = z - cz;
      weight += expf(-0.5f * (rx * rx + rz * rz) * inv_sigma2);
    }
  }
  return amplitude * weight;
}

/* World-space clump identity. Both patch density and blade placement use
 * the same deterministic centers; clump_radius never enters this choice. */
static Vec2 s_clump_center(int cell_x, int cell_z)
{
  const float cell_size = 3.0f;
  u32 state = s_random_state(
      (u32)cell_x * 0x9e3779b9u ^
      (u32)cell_z * 0x85ebca6bu ^ 0x73d45b91u);
  return vec2_make(
      ((float)cell_x + 0.15f + 0.70f * s_random_unit(&state)) * cell_size,
      ((float)cell_z + 0.15f + 0.70f * s_random_unit(&state)) * cell_size);
}

/* Select a center based on the existing density kernels, without consulting
 * clump_radius. Neighboring patches therefore share the same clump roots. */
static Vec2 s_clump_center_at(float x, float z, u32 *random)
{
  const float cell_size = 3.0f;
  const float inv_sigma2 = 1.0f / (0.65f * 0.65f);
  int cell_x = (int)floorf(x / cell_size);
  int cell_z = (int)floorf(z / cell_size);
  float total = 0.0f;
  Vec2 chosen = vec2_make(x, z);

  for (int dz = -2; dz <= 2; ++dz)
  {
    for (int dx = -2; dx <= 2; ++dx)
    {
      Vec2 center = s_clump_center(cell_x + dx, cell_z + dz);
      float rx = x - center.x;
      float rz = z - center.y;
      float weight = expf(-0.5f * (rx * rx + rz * rz) * inv_sigma2);
      total += weight;
      if (s_random_unit(random) * total < weight)
      {
        chosen = center;
      }
    }
  }
  return chosen;
}

static float s_clumping_patch_weight(const LDKGrassPlane *plane)
{
  static const float samples[5][2] = {
      {0.5f, 0.5f}, {0.2f, 0.2f}, {0.8f, 0.2f},
      {0.2f, 0.8f}, {0.8f, 0.8f}};
  float result = 0.0f;

  for (u32 i = 0u; i < 5u; ++i)
  {
    Vec3 point = vec3_add(plane->origin,
        vec3_add(vec3_mul(plane->axis_u, samples[i][0]),
            vec3_mul(plane->axis_v, samples[i][1])));
    result += s_clumping_density_at(point.x, point.z);
  }
  return result * 0.2f;
}

static bool s_topology_is_valid(LDKGrassBladeTopology topology)
{
  return topology == LDK_GRASS_BLADE_TOPOLOGY_RIBBON ||
         topology == LDK_GRASS_BLADE_TOPOLOGY_CROSSED_RIBBONS ||
         topology == LDK_GRASS_BLADE_TOPOLOGY_RADIAL ||
         topology == LDK_GRASS_BLADE_TOPOLOGY_TRIPLE_RIBBONS ||
         topology == LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM ||
         topology == LDK_GRASS_BLADE_TOPOLOGY_PLUMED_STEM ||
         topology == LDK_GRASS_BLADE_TOPOLOGY_CUSTOM;
}

static bool s_shape_is_valid(const LDKGrassBladeShape *shape)
{
  if (!shape || !s_topology_is_valid(shape->topology) ||
      (shape->topology == LDK_GRASS_BLADE_TOPOLOGY_CUSTOM &&
          shape->custom_slot >= LDK_GRASS_CUSTOM_BLADE_PRESET_COUNT) ||
      ((shape->topology == LDK_GRASS_BLADE_TOPOLOGY_RADIAL ||
           shape->topology == LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM) &&
          (shape->side_count < LDK_GRASS_MIN_SIDE_COUNT ||
              shape->side_count > LDK_GRASS_MAX_SIDE_COUNT)))
  {
    return false;
  }

  for (u32 i = 0u; i < LDK_GRASS_PROFILE_POINT_COUNT; ++i)
  {
    const LDKGrassProfilePoint *point = &shape->profile[i];

    if (!isfinite(point->height) || !isfinite(point->width) ||
        point->height < 0.0f || point->height > 1.0f ||
        point->width < 0.0f ||
        (i + 1u < LDK_GRASS_PROFILE_POINT_COUNT && point->width <= 0.0f))
    {
      return false;
    }

    if (i > 0u && point->height <= shape->profile[i - 1u].height)
    {
      return false;
    }
  }

  return shape->profile[0].height == 0.0f &&
         shape->profile[0].width > 0.0f &&
         shape->profile[LDK_GRASS_PROFILE_POINT_COUNT - 1u].height == 1.0f;
}

static bool s_desc_is_valid(
    const LDKGrassResolvedDesc *desc, float *out_area, Vec3 *out_normal)
{
  Vec3 cross;
  float area;
  double expected_count;

  if (!desc || !s_vec3_is_finite(desc->plane.origin) ||
      !s_vec3_is_finite(desc->plane.axis_u) ||
      !s_vec3_is_finite(desc->plane.axis_v) ||
      !isfinite(desc->density) || desc->density < 0.0f ||
      !isfinite(desc->clumping) || desc->clumping < 0.0f ||
      desc->clumping > 1.0f ||
      !isfinite(desc->clump_radius) || desc->clump_radius < 0.0f ||
      !isfinite(desc->min_height) || desc->min_height <= 0.0f ||
      !isfinite(desc->max_height) ||
      desc->max_height < desc->min_height ||
      !isfinite(desc->min_tilt) || desc->min_tilt < 0.0f ||
      !isfinite(desc->max_tilt) || desc->max_tilt < desc->min_tilt ||
      desc->max_tilt >= LDK_GRASS_PI * 0.5f ||
      !isfinite(desc->tilt_direction) ||
      !isfinite(desc->specular) || desc->specular < 0.0f ||
      !isfinite(desc->shininess) || desc->shininess < 0.0f ||
      !isfinite(desc->emission) || desc->emission < 0.0f ||
      !isfinite(desc->curvature) || desc->curvature < 0.0f ||
      !isfinite(desc->wind_strength) || desc->wind_strength < 0.0f ||
      !isfinite(desc->wind_speed) || desc->wind_speed < 0.0f ||
      !isfinite(desc->wind_direction) ||
      !isfinite(desc->interaction_recovery_time) ||
      desc->interaction_recovery_time <= 0.0f ||
      desc->interaction_recovery_time >
          LDK_GRASS_INTERACTION_MAX_RECOVERY_TIME ||
      !s_shape_is_valid(&desc->shape))
  {
    return false;
  }

  cross = vec3_cross(desc->plane.axis_u, desc->plane.axis_v);
  area = vec3_len(cross);
  expected_count = (double)area * (double)desc->density;
  if (!isfinite(area) || area <= STDXM_EPS ||
      expected_count > (double)UINT32_MAX)
  {
    return false;
  }

  if (out_area)
  {
    *out_area = area;
  }
  if (out_normal)
  {
    *out_normal = vec3_div(cross, area);
  }
  return true;
}

static void s_bounds_add_point(Vec3 *bounds_min, Vec3 *bounds_max, Vec3 point)
{
  bounds_min->x = float_min(bounds_min->x, point.x);
  bounds_min->y = float_min(bounds_min->y, point.y);
  bounds_min->z = float_min(bounds_min->z, point.z);
  bounds_max->x = float_max(bounds_max->x, point.x);
  bounds_max->y = float_max(bounds_max->y, point.y);
  bounds_max->z = float_max(bounds_max->z, point.z);
}

/* Renderer and gameplay use the same authored-mesh source for decorations.
 * Missing meshes keep their patches but are not submitted for drawing. */
static const LDKMeshData *s_decoration_mesh_get(
    const LDKAssetPath *path, u32 mesh_index)
{
  if (!path || !path->length) return NULL;
  LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!assets) return NULL;
  LDKAssetMesh mesh = ldk_asset_manager_mesh_load_shared(
      assets, path->buf, NULL);
  return ldk_asset_manager_mesh_data_at(assets, mesh, mesh_index);
}

static void s_patch_bounds_calculate(const LDKGrassResolvedDesc *desc,
    const Mat4 *instances, u32 instance_count,
    Vec3 *out_bounds_min, Vec3 *out_bounds_max)
{
  if (desc->kind != LDK_GRASS_LAYER_VEGETATION)
  {
    const LDKMeshData *mesh = s_decoration_mesh_get(
        &desc->mesh_path, desc->mesh_index);
    Vec3 low = vec3_make(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 high = vec3_make(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    if (mesh && mesh->vertices && mesh->vertex_count && instance_count)
    {
      /* Build local bounds once. Transforming their eight corners gives a
       * conservative world AABB even for randomly rotated meshes, without
       * scanning every vertex separately for every instance. */
      Vec3 mesh_min = vec3_make(FLT_MAX, FLT_MAX, FLT_MAX);
      Vec3 mesh_max = vec3_make(-FLT_MAX, -FLT_MAX, -FLT_MAX);
      for (u32 j = 0u; j < mesh->vertex_count; ++j)
        s_bounds_add_point(&mesh_min, &mesh_max,
            mesh->vertices[j].position);
      for (u32 i = 0u; i < instance_count; ++i)
        for (u32 corner = 0u; corner < 8u; ++corner)
        {
          Vec3 local = vec3_make(
              corner & 1u ? mesh_max.x : mesh_min.x,
              corner & 2u ? mesh_max.y : mesh_min.y,
              corner & 4u ? mesh_max.z : mesh_min.z);
          s_bounds_add_point(&low, &high,
              mat4_mul_point(instances[i], local));
        }
    }
    if (low.x == FLT_MAX)
    {
      Vec3 corners[4];
      corners[0] = desc->plane.origin;
      corners[1] = vec3_add(corners[0], desc->plane.axis_u);
      corners[2] = vec3_add(corners[0], desc->plane.axis_v);
      corners[3] = vec3_add(corners[1], desc->plane.axis_v);
      for (u32 i = 0u; i < 4u; ++i)
        s_bounds_add_point(&low, &high, corners[i]);
    }
    /* Tree arc bending can move tips beyond the rigid mesh AABB. */
    float wind_extent = desc->kind == LDK_GRASS_LAYER_TREE
        ? desc->wind_strength * (high.y - low.y) * 2.0f : 0.0f;
    Vec3 padding = vec3_make(wind_extent, wind_extent, wind_extent);
    *out_bounds_min = vec3_sub(low, padding);
    *out_bounds_max = vec3_add(high, padding);
    return;
  }

  Vec3 bounds_min = vec3_make(FLT_MAX, FLT_MAX, FLT_MAX);
  Vec3 bounds_max = vec3_make(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  float half_width = 0.0f;
  float bend_extent;

  for (u32 i = 0u; i < LDK_GRASS_PROFILE_POINT_COUNT; ++i)
  {
    half_width = float_max(
        half_width, desc->shape.profile[i].width * 0.5f);
  }
  if (desc->shape.topology == LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM)
  {
    half_width += desc->shape.profile[0].width *
        (LDK_GRASS_THORNY_BEND_MAX_SCALE +
            LDK_GRASS_THORN_MAX_LENGTH_SCALE);
  }
  else if (desc->shape.topology == LDK_GRASS_BLADE_TOPOLOGY_PLUMED_STEM)
  {
    /* The plume tips extend sideways beyond the central stem. */
    half_width = desc->shape.profile[0].width * 5.0f;
  }

  /* An affine transform of a box has an exact axis-aligned bounding box.
   * Computing its center and extent directly avoids transforming all eight
   * corners of every grass blade during a regional editor stroke. */
  for (u32 i = 0u; i < instance_count; ++i)
  {
    const float *m = instances[i].m;
    const float cx = m[12] + 0.5f * m[4];
    const float cy = m[13] + 0.5f * m[5];
    const float cz = m[14] + 0.5f * m[6];
    const float ex = half_width * (fabsf(m[0]) + fabsf(m[8])) +
        0.5f * fabsf(m[4]);
    const float ey = half_width * (fabsf(m[1]) + fabsf(m[9])) +
        0.5f * fabsf(m[5]);
    const float ez = half_width * (fabsf(m[2]) + fabsf(m[10])) +
        0.5f * fabsf(m[6]);
    bounds_min.x = fminf(bounds_min.x, cx - ex);
    bounds_min.y = fminf(bounds_min.y, cy - ey);
    bounds_min.z = fminf(bounds_min.z, cz - ez);
    bounds_max.x = fmaxf(bounds_max.x, cx + ex);
    bounds_max.y = fmaxf(bounds_max.y, cy + ey);
    bounds_max.z = fmaxf(bounds_max.z, cz + ez);
  }

  if (instance_count == 0u)
  {
    Vec3 corners[4];

    corners[0] = desc->plane.origin;
    corners[1] = vec3_add(desc->plane.origin, desc->plane.axis_u);
    corners[2] = vec3_add(desc->plane.origin, desc->plane.axis_v);
    corners[3] = vec3_add(corners[1], desc->plane.axis_v);
    for (u32 i = 0u; i < 4u; ++i)
    {
      s_bounds_add_point(&bounds_min, &bounds_max, corners[i]);
    }
  }

  bend_extent = desc->max_height *
      (desc->curvature + desc->wind_strength);
  bounds_min = vec3_sub(bounds_min,
      vec3_make(bend_extent, bend_extent, bend_extent));
  bounds_max = vec3_add(bounds_max,
      vec3_make(bend_extent, bend_extent, bend_extent));
  *out_bounds_min = bounds_min;
  *out_bounds_max = bounds_max;
}

static bool s_shape_equal(
    const LDKGrassBladeShape *a, const LDKGrassBladeShape *b)
{
  if (!a || !b || a->topology != b->topology ||
      (a->topology == LDK_GRASS_BLADE_TOPOLOGY_CUSTOM &&
          a->custom_slot != b->custom_slot) ||
      (a->topology == LDK_GRASS_BLADE_TOPOLOGY_RADIAL &&
          a->side_count != b->side_count))
  {
    return false;
  }

  for (u32 i = 0u; i < LDK_GRASS_PROFILE_POINT_COUNT; ++i)
  {
    if (a->profile[i].height != b->profile[i].height ||
        a->profile[i].width != b->profile[i].width)
    {
      return false;
    }
  }
  return true;
}

static bool s_batch_compatible(
    const LDKGrassBatch *batch, const LDKGrassResolvedDesc *desc)
{
  return batch && desc &&
         batch->kind == desc->kind &&
         (desc->kind == LDK_GRASS_LAYER_VEGETATION ||
             (batch->mesh_index == desc->mesh_index &&
                 strcmp(batch->mesh_path.buf, desc->mesh_path.buf) == 0 &&
                 memcmp(batch->material_paths, desc->material_paths,
                     sizeof(batch->material_paths)) == 0)) &&
         s_shape_equal(&batch->shape, &desc->shape) &&
         batch->bottom_color == desc->bottom_color &&
         batch->top_color == desc->top_color &&
         batch->use_noise_color == desc->use_noise_color &&
         batch->noise_color_stop_0_color == desc->noise_color_stop_0_color &&
         batch->noise_color_stop_1_color == desc->noise_color_stop_1_color &&
         batch->noise_color_stop_2_color == desc->noise_color_stop_2_color &&
         batch->noise_color_stop_3_color == desc->noise_color_stop_3_color &&

         batch->specular == desc->specular &&
         batch->shininess == s_shininess_resolve(desc->shininess) &&
         batch->emission == desc->emission &&
         batch->curvature == desc->curvature &&
         batch->wind_strength == desc->wind_strength &&
         batch->wind_speed == desc->wind_speed &&
         batch->wind_direction == desc->wind_direction &&
         batch->interaction_recovery_time ==
             desc->interaction_recovery_time &&
         batch->casts_shadows == desc->casts_shadows &&
         batch->unlit == desc->unlit;
}

static bool s_patches_reserve(u32 required)
{
  LDKGrassPatchEntry *patches;
  u32 capacity;

  if (required <= s_grass.patch_capacity)
  {
    return true;
  }

  capacity = s_grass.patch_capacity ? s_grass.patch_capacity : 64u;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }

  if (!s_allocation_size_is_valid(capacity, sizeof(*patches)))
  {
    return false;
  }

  patches = (LDKGrassPatchEntry *)realloc(
      s_grass.patches, (size_t)capacity * sizeof(*patches));
  if (!patches)
  {
    return false;
  }

  memset(patches + s_grass.patch_capacity, 0,
      (size_t)(capacity - s_grass.patch_capacity) * sizeof(*patches));
  s_grass.patches = patches;
  s_grass.patch_capacity = capacity;
  return true;
}

static bool s_batches_reserve(u32 required)
{
  LDKGrassBatch *batches;
  u32 capacity;

  if (required <= s_grass.batch_capacity)
  {
    return true;
  }

  capacity = s_grass.batch_capacity ? s_grass.batch_capacity : 8u;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }

  if (!s_allocation_size_is_valid(capacity, sizeof(*batches)))
  {
    return false;
  }

  batches = (LDKGrassBatch *)realloc(
      s_grass.batches, (size_t)capacity * sizeof(*batches));
  if (!batches)
  {
    return false;
  }

  memset(batches + s_grass.batch_capacity, 0,
      (size_t)(capacity - s_grass.batch_capacity) * sizeof(*batches));
  s_grass.batches = batches;
  s_grass.batch_capacity = capacity;
  return true;
}

static void s_batch_release(LDKGrassBatch *batch);

static u32 s_batch_find_or_add(const LDKGrassResolvedDesc *desc)
{
  LDKGrassBatch *batch;

  for (u32 i = 0u; i < s_grass.batch_count; ++i)
  {
    if (s_batch_compatible(&s_grass.batches[i], desc))
    {
      return i;
    }
  }

  /* Reuse a batch no longer referenced by any live patch. Slider edits
   * otherwise accumulate a new GPU mesh/material for every intermediate
   * value until the next full GrassSystem rebuild. */
  u32 slot = s_grass.batch_count;
  for (u32 i = 0u; i < s_grass.batch_count; ++i)
  {
    bool referenced = false;
    for (u32 j = 0u; j < s_grass.patch_count; ++j)
      if (s_grass.patches[j].alive &&
          s_grass.patches[j].batch_index == i)
      {
        referenced = true;
        break;
      }
    if (!referenced) {slot = i; break;}
  }
  if (slot == s_grass.batch_count &&
      !s_batches_reserve(s_grass.batch_count + 1u))
    return LDK_GRASS_INVALID_BATCH;

  batch = &s_grass.batches[slot];
  if (slot < s_grass.batch_count)
    s_batch_release(batch);
  memset(batch, 0, sizeof(*batch));
  batch->shape = desc->shape;
  batch->kind = desc->kind;
  batch->mesh_path = desc->mesh_path;
  batch->mesh_index = desc->mesh_index;
  memcpy(batch->material_paths, desc->material_paths,
      sizeof(batch->material_paths));
  batch->bottom_color = desc->bottom_color;
  batch->top_color = desc->top_color;
  batch->use_noise_color = desc->use_noise_color;
  batch->noise_color_stop_0_color = desc->noise_color_stop_0_color;
  batch->noise_color_stop_1_color = desc->noise_color_stop_1_color;
  batch->noise_color_stop_2_color = desc->noise_color_stop_2_color;
  batch->noise_color_stop_3_color = desc->noise_color_stop_3_color;

  batch->specular = desc->specular;
  batch->shininess = s_shininess_resolve(desc->shininess);
  batch->emission = desc->emission;
  batch->curvature = desc->curvature;
  batch->wind_strength = desc->wind_strength;
  batch->wind_speed = desc->wind_speed;
  batch->wind_direction = desc->wind_direction;
  batch->interaction_recovery_time = desc->interaction_recovery_time;
  batch->casts_shadows = desc->casts_shadows;
  batch->unlit = desc->unlit;
  batch->mesh = ldk_renderer_mesh_null();
  batch->lod_mesh = ldk_renderer_mesh_null();
  batch->material = ldk_renderer_material_null();
  batch->instance_set = ldk_renderer_instance_set_null();
  batch->lod_instance_set = ldk_renderer_instance_set_null();
  batch->lod_dirty = true;
  batch->dirty = true;
  if (slot == s_grass.batch_count) ++s_grass.batch_count;
  return slot;
}

static float s_noise_sample(float x, float z);

typedef struct LDKGrassTerrainPlacement
{
  const LDKTerrain *terrain;
  LDKImageInfo heights;
  LDKImageInfo density;
  const u8 *density_samples;
  bool has_heights;
  bool has_density;
  float origin_x;
  float origin_z;
  Mat4 world;
  Mat4 inverse;
  bool has_entity_transform;
} LDKGrassTerrainPlacement;

/* A zero-initialized optional handle is also treated as unassigned. */
static bool s_asset_handle_assigned(XHandle handle)
{
  return !x_handle_is_null(handle) &&
      !(handle.index == 0u && handle.version == 0u);
}

static bool s_terrain_placement_resolve(const LDKGrassResolvedDesc *desc,
    LDKGrassTerrainPlacement *out)
{
  LDKAssetManager *assets;
  const LDKAssetImageData *image;
  if (!out || !desc) return false;
  memset(out, 0, sizeof(*out));
  if (x_handle_is_null(desc->terrain_entity)) return true;

  const LDKTerrainComponent *component = ldk_ecs_component_get_const(
      desc->terrain_entity, LDK_COMPONENT_TYPE_TERRAIN);
  bool inverse_ok = false;
  if (!component || !ldk_ecs_component_is_enabled(
          desc->terrain_entity, LDK_COMPONENT_TYPE_TERRAIN) ||
      !ldk_terrain_validate(&component->terrain) ||
      !ldk_scenegraph_update_entity(desc->terrain_entity) ||
      !ldk_transform_get_world_matrix(desc->terrain_entity, &out->world))
    return false;
  out->inverse = mat4_inverse_full(out->world, &inverse_ok);
  if (!inverse_ok) return false;
  out->has_entity_transform = true;
  out->terrain = &component->terrain;
  out->origin_x = component->terrain.origin_x;
  out->origin_z = component->terrain.origin_z;
  if (component->terrain.center_on_origin)
  {
    out->origin_x -= (float)component->terrain.width *
        component->terrain.cell_size * 0.5f;
    out->origin_z -= (float)component->terrain.height *
        component->terrain.cell_size * 0.5f;
  }
  if (component->heightmap)
  {
    if (!ldk_image_get_info(component->heightmap, &out->heights) ||
        out->heights.width != component->terrain.width ||
        out->heights.height != component->terrain.height)
      return false;
    out->has_heights = true;
  }
  if (desc->terrain_vegetation_layer_id)
  {
    out->density_samples = ldk_terrain_component_vegetation_density_get(
        component, desc->terrain_vegetation_layer_id);
    if (!out->density_samples) return false;
    out->has_density = true;
    return true;
  }
  assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (s_asset_handle_assigned(desc->density_mask.h))
  {
    image = ldk_asset_manager_image_get_const(assets, desc->density_mask);
    if (!image || !image->image || image->is_missing ||
        !ldk_image_get_info(image->image, &out->density) ||
        !out->density.width || !out->density.height ||
        out->density.channel_count != 4u ||
        (out->density.format != LDK_IMAGE_FORMAT_RGBA8_UNORM &&
         out->density.format != LDK_IMAGE_FORMAT_RGBA16_UNORM))
      return false;
    out->has_density = true;
  }
  return true;
}

static float s_terrain_mask_texel(
    const LDKImageInfo *image, u32 x, u32 z)
{
  size_t index = ((size_t)z * image->width + x) * 4u;
  if (image->format == LDK_IMAGE_FORMAT_RGBA16_UNORM)
  {
    u16 v;
    memcpy(&v, image->pixels + index * sizeof(v), sizeof(v));
    return (float)v * (1.0f / 65535.0f);
  }
  return (float)image->pixels[index] * (1.0f / 255.0f);
}

/* Pixel-centered, clamped bilinear sampling. Both the texture resolution and
 * terrain cell size are independent; this works with low-res paint masks. */
static float s_terrain_mask_sample(
    const LDKGrassTerrainPlacement *p, float x, float z)
{
  if (!p->has_density)
  {
    return 1.0f;
  }
  const LDKImageInfo *m = &p->density;
  /* Embedded masks use one u8 sample per terrain grid cell. */
  const u32 width = p->density_samples
      ? p->terrain->width : m->width;
  const u32 height = p->density_samples
      ? p->terrain->height : m->height;
  float u = s_clamp01((x - p->origin_x) /
      ((float)p->terrain->width * p->terrain->cell_size));
  float v = s_clamp01((z - p->origin_z) /
      ((float)p->terrain->height * p->terrain->cell_size));
  float fx = fmaxf(0.0f, fminf((float)width - 1.0f,
      u * (float)width - 0.5f));
  float fz = fmaxf(0.0f, fminf((float)height - 1.0f,
      v * (float)height - 0.5f));
  u32 x0 = (u32)floorf(fx);
  u32 z0 = (u32)floorf(fz);
  u32 x1 = x0 + (x0 + 1u < width ? 1u : 0u);
  u32 z1 = z0 + (z0 + 1u < height ? 1u : 0u);
  float tx = fx - (float)x0;
  float tz = fz - (float)z0;
  float a, b, c, d;
  if (p->density_samples)
  {
    a = p->density_samples[(size_t)z0 * width + x0] / 255.0f;
    b = p->density_samples[(size_t)z0 * width + x1] / 255.0f;
    c = p->density_samples[(size_t)z1 * width + x0] / 255.0f;
    d = p->density_samples[(size_t)z1 * width + x1] / 255.0f;
  }
  else
  {
    a = s_terrain_mask_texel(m, x0, z0);
    b = s_terrain_mask_texel(m, x1, z0);
    c = s_terrain_mask_texel(m, x0, z1);
    d = s_terrain_mask_texel(m, x1, z1);
  }
  return (a + (b - a) * tx) * (1.0f - tz) +
      (c + (d - c) * tx) * tz;
}

typedef struct LDKGrassTerrainClip
{
  float min_x;
  float min_z;
  float max_x;
  float max_z;
} LDKGrassTerrainClip;

static bool s_terrain_clip_contains(const LDKGrassTerrainClip *clip,
    float x, float z)
{
  return !clip || (x >= clip->min_x && x <= clip->max_x &&
      z >= clip->min_z && z <= clip->max_z);
}

static void s_terrain_candidate_index_destroy(
    LDKGrassTerrainCandidateIndex *index)
{
  if (!index)
  {
    return;
  }
  free(index->candidates);
  free(index->candidate_slots);
  free(index->slot_candidates);
  free(index);
}

static LDKGrassTerrainCandidateIndex *s_terrain_candidate_index_make(
    u32 count)
{
  if (!count || !s_allocation_size_is_valid(
          count, sizeof(LDKGrassTerrainCandidate)))
  {
    return NULL;
  }
  LDKGrassTerrainCandidateIndex *index = calloc(1u, sizeof(*index));
  if (!index)
  {
    return NULL;
  }
  index->candidates = malloc((size_t)count * sizeof(*index->candidates));
  index->candidate_slots = malloc((size_t)count * sizeof(u32));
  index->slot_candidates = malloc((size_t)count * sizeof(u32));
  if (!index->candidates || !index->candidate_slots ||
      !index->slot_candidates)
  {
    s_terrain_candidate_index_destroy(index);
    return NULL;
  }
  index->count = count;
  for (u32 i = 0u; i < count; ++i)
    index->candidate_slots[i] = UINT32_MAX;
  for (u32 i = 0u; i < LDK_GRASS_TERRAIN_CANDIDATE_BIN_COUNT; ++i)
  {
    index->heads[i] = UINT32_MAX;
  }
  return index;
}

static u32 s_terrain_candidate_bin(float coord)
{
  if (!(coord > 0.0f))
  {
    return 0u;
  }
  if (coord >= 1.0f)
  {
    return LDK_GRASS_TERRAIN_CANDIDATE_BINS - 1u;
  }
  return (u32)(coord * (float)LDK_GRASS_TERRAIN_CANDIDATE_BINS);
}

static u32 s_terrain_candidate_bucket(const LDKGrassPlane *plane,
    float x, float z)
{
  const u32 bx = s_terrain_candidate_bin(
      (x - plane->origin.x) / plane->axis_u.x);
  const u32 bz = s_terrain_candidate_bin(
      (plane->origin.z - z) / -plane->axis_v.z);
  return bz * LDK_GRASS_TERRAIN_CANDIDATE_BINS + bx;
}

static void s_terrain_candidate_index_insert(
    LDKGrassTerrainCandidateIndex *index, const LDKGrassPlane *plane,
    u32 id, Vec3 position, u32 random_state)
{
  const u32 bucket = s_terrain_candidate_bucket(
      plane, position.x, position.z);
  LDKGrassTerrainCandidate *item = &index->candidates[id];
  item->position = vec2_make(position.x, position.z);
  item->random_state = random_state;
  item->next = index->heads[bucket];
  index->heads[bucket] = id;
}

typedef struct LDKGrassTerrainCandidateIterator
{
  const LDKGrassTerrainCandidateIndex *index;
  u32 min_x;
  u32 max_x;
  u32 max_z;
  u32 x;
  u32 z;
  u32 next;
} LDKGrassTerrainCandidateIterator;

static void s_terrain_candidate_iterator_init(
    LDKGrassTerrainCandidateIterator *it,
    const LDKGrassTerrainCandidateIndex *index,
    const LDKGrassPlane *plane, const LDKGrassTerrainClip *clip)
{
  const float size_x = plane->axis_u.x;
  const float size_z = -plane->axis_v.z;
  it->index = index;
  it->min_x = s_terrain_candidate_bin(
      (clip->min_x - plane->origin.x) / size_x);
  it->max_x = s_terrain_candidate_bin(
      (clip->max_x - plane->origin.x) / size_x);
  it->z = s_terrain_candidate_bin(
      (plane->origin.z - clip->max_z) / size_z);
  it->max_z = s_terrain_candidate_bin(
      (plane->origin.z - clip->min_z) / size_z);
  it->x = it->min_x;
  it->next = UINT32_MAX;
}

static bool s_terrain_candidate_iterator_next(
    LDKGrassTerrainCandidateIterator *it, Vec3 *position,
    u32 *random_state, u32 *candidate_id)
{
  while (it->next == UINT32_MAX)
  {
    if (it->z > it->max_z)
    {
      return false;
    }
    const u32 bucket = it->z * LDK_GRASS_TERRAIN_CANDIDATE_BINS + it->x;
    it->next = it->index->heads[bucket];
    if (it->x == it->max_x)
    {
      it->x = it->min_x;
      ++it->z;
    }
    else
    {
      ++it->x;
    }
  }
  const u32 id = it->next;
  const LDKGrassTerrainCandidate *item = &it->index->candidates[id];
  it->next = item->next;
  *position = vec3_make(item->position.x, 0.0f, item->position.y);
  *random_state = item->random_state;
  if (candidate_id) *candidate_id = id;
  return true;
}

static bool s_patch_instances_generate(const LDKGrassResolvedDesc *desc,
    Mat4 **out_instances, Vec3 **out_normals, u32 *out_count,
    const LDKGrassTerrainClip *clip,
    LDKGrassTerrainCandidateIndex **index_io,
    u32 **out_candidate_ids)
{
  Mat4 *instances = NULL;
  Vec3 *normals = NULL;
  u32 normals_capacity = 0u;
  Vec3 normal;
  float area;
  double expected_count;
  u32 count;
  u32 random;
  u32 accepted = 0u;
  u32 instance_capacity = 0u;
  Quat align;
  LDKGrassTerrainPlacement placement;
  LDKGrassTerrainCandidateIndex *index = NULL;
  LDKGrassTerrainCandidateIterator iterator = {0};
  u32 *partial_ids = NULL;
  u32 partial_capacity = 0u;
  bool use_index = false;

  if (!out_instances || !out_normals || !out_count ||
      !s_desc_is_valid(desc, &area, &normal) ||
      !s_terrain_placement_resolve(desc, &placement))
  {
    return false;
  }

  *out_instances = NULL;
  *out_normals = NULL;
  *out_count = 0u;
  if (out_candidate_ids) *out_candidate_ids = NULL;
  random = s_random_state(desc->seed);
  expected_count = (double)area * (double)desc->density;
  if (desc->clumping > 0.0f)
  {
    float weight = s_clumping_patch_weight(&desc->plane);
    expected_count *= (double)(1.0f + desc->clumping * (weight - 1.0f));
    if (expected_count > (double)UINT32_MAX - 1.0)
    {
      return false;
    }
  }
  count = (u32)floor(expected_count);
  if ((double)s_random_unit(&random) < expected_count - (double)count)
  {
    count += 1u;
  }

  if (count == 0u)
  {
    if (index_io)
    {
      s_terrain_candidate_index_destroy(*index_io);
      *index_io = NULL;
    }
    return true;
  }
  if (!s_allocation_size_is_valid(count, sizeof(*instances)))
  {
    return false;
  }

  /* The full scatter builds a spatial index as it already visits every
   * candidate. Later brush strokes visit only bins that overlap the mask
   * rectangle. Non-terrain patches keep the original generation path. */
  if (index_io && placement.terrain &&
      desc->terrain_vegetation_layer_id &&
      desc->plane.axis_u.x > STDXM_EPS &&
      desc->plane.axis_v.z < -STDXM_EPS)
  {
    index = *index_io;
    if (index && index->count != count)
    {
      s_terrain_candidate_index_destroy(index);
      index = NULL;
      *index_io = NULL;
    }
    if (!index)
    {
      index = s_terrain_candidate_index_make(count);
      *index_io = index;
    }
    use_index = index && index->ready && clip;
    if (use_index)
      s_terrain_candidate_iterator_init(&iterator, index,
          &desc->plane, clip);
  }

  /* A regional update may accept only a tiny fraction of the candidates.
   * Do not allocate one full-patch array for every brush sample. */
  instance_capacity = clip && placement.terrain
      ? (count < 64u ? count : 64u) : count;
  instances = (Mat4 *)malloc((size_t)instance_capacity * sizeof(*instances));
  if (!instances) return false;

  align = quat_from_to(vec3_make(0.0f, 1.0f, 0.0f), normal);
  for (u32 i = 0u;; ++i)
  {
    u32 candidate_random = 0u;
    u32 candidate_id = i;
    Vec3 position;
    if (use_index)
    {
      if (!s_terrain_candidate_iterator_next(
              &iterator, &position, &candidate_random,
              &candidate_id)) break;
    }
    else
    {
      if (i >= count) break;
      /* A candidate owns its random sequence. This also allows the cache to
       * reproduce its mask decision, height and rotation independently. */
      candidate_random = s_random_state(
          desc->seed ^ (0x9e3779b9u * (i + 1u)));
      u32 *position_rng = placement.terrain ? &candidate_random : &random;
      float u = s_random_unit(position_rng);
      float v = s_random_unit(position_rng);
      if (desc->clumping > 0.0f &&
          s_random_unit(position_rng) < desc->clumping)
      {
        Vec3 sample = vec3_add(desc->plane.origin,
            vec3_add(vec3_mul(desc->plane.axis_u, u),
                vec3_mul(desc->plane.axis_v, v)));
        Vec2 center = s_clump_center_at(sample.x, sample.z, position_rng);
        float theta = s_random_unit(position_rng) * 2.0f * LDK_GRASS_PI;
        float distance = desc->clump_radius * sqrtf(s_random_unit(position_rng));
        Vec3 target = vec3_make(center.x + cosf(theta) * distance,
            sample.y, center.y + sinf(theta) * distance);
        Vec3 offset = vec3_sub(target, desc->plane.origin);
        Vec3 axis_u = desc->plane.axis_u;
        Vec3 axis_v = desc->plane.axis_v;
        float uu = vec3_dot(axis_u, axis_u);
        float uv = vec3_dot(axis_u, axis_v);
        float vv = vec3_dot(axis_v, axis_v);
        float det = uu * vv - uv * uv;

        /* Do not clamp to patch edges; clumps can cross patch borders. */
        if (det > STDXM_EPS)
        {
          float du = vec3_dot(offset, axis_u);
          float dv = vec3_dot(offset, axis_v);
          u = (du * vv - dv * uv) / det;
          v = (dv * uu - du * uv) / det;
        }
      }
      position = vec3_add(desc->plane.origin,
          vec3_add(vec3_mul(desc->plane.axis_u, u),
              vec3_mul(desc->plane.axis_v, v)));
      if (index && !index->ready)
        s_terrain_candidate_index_insert(index,
            &desc->plane, i, position, candidate_random);
    }
    u32 *rng = placement.terrain ? &candidate_random : &random;
    if (clip && placement.terrain && !s_terrain_clip_contains(
            clip, position.x, position.z)) continue;
    /* A regional density refresh must generate only newly accepted
     * candidates. Existing instances are kept or removed by the caller
     * after testing their original random threshold against the new mask. */
    if (use_index && out_candidate_ids &&
        index->candidate_slots[candidate_id] != UINT32_MAX)
      continue;
    Vec3 ground_normal = vec3_make(0.0f, 1.0f, 0.0f);
    if (placement.terrain)
    {
      float ground_y;
      /* Reject candidates outside painted density first. This avoids an
       * expensive height/normal sample for the overwhelmingly empty parts
       * of sparse vegetation masks, without changing candidate RNG order. */
      if (s_random_unit(rng) >
              s_terrain_mask_sample(&placement, position.x, position.z) ||
          !ldk_terrain_surface_sample(placement.terrain,
              placement.has_heights ? &placement.heights : NULL,
              position.x, position.z, &ground_y, &ground_normal))
      {
        continue;
      }
      position.y = desc->kind != LDK_GRASS_LAYER_VEGETATION && !desc->follow_terrain
          ? 0.0f : ground_y;
      if (placement.has_entity_transform)
      {
        Vec3 world_normal = vec3_make(
            placement.inverse.m[0] * ground_normal.x +
                placement.inverse.m[1] * ground_normal.y +
                placement.inverse.m[2] * ground_normal.z,
            placement.inverse.m[4] * ground_normal.x +
                placement.inverse.m[5] * ground_normal.y +
                placement.inverse.m[6] * ground_normal.z,
            placement.inverse.m[8] * ground_normal.x +
                placement.inverse.m[9] * ground_normal.y +
                placement.inverse.m[10] * ground_normal.z);
        ground_normal = vec3_norm(world_normal);
        position = mat4_mul_point(placement.world, position);
      }
      align = quat_from_to(vec3_make(0.0f, 1.0f, 0.0f), ground_normal);
    }
    float height_sample = s_random_unit(rng);
    float angle = s_random_unit(rng) * 2.0f * LDK_GRASS_PI;
    float tilt_angle = desc->min_tilt +
        (desc->max_tilt - desc->min_tilt) * s_random_unit(rng);
    if (desc->use_noise_height)
    {
      height_sample = s_noise_sample(position.x, position.z);
    }
    float height = desc->min_height +
        (desc->max_height - desc->min_height) * height_sample;
    Quat yaw =
        quat_axis_angle(vec3_make(0.0f, 1.0f, 0.0f), angle);
    Quat tilt = quat_axis_angle(
        vec3_make(sinf(desc->tilt_direction), 0.0f,
            -cosf(desc->tilt_direction)),
        tilt_angle);
    Quat rotation = quat_mul(align, quat_mul(tilt, yaw));

    if (accepted == instance_capacity)
    {
      u32 capacity = instance_capacity <= UINT32_MAX / 2u
          ? instance_capacity * 2u : count;
      if (capacity > count) capacity = count;
      if (capacity <= instance_capacity ||
          !s_allocation_size_is_valid(capacity, sizeof(*instances)))
      {
        free(normals);
        free(instances);
        free(partial_ids);
        return false;
      }
      Mat4 *grown = realloc(instances,
          (size_t)capacity * sizeof(*instances));
      if (!grown)
      {
        free(normals);
        free(instances);
        free(partial_ids);
        return false;
      }
      instances = grown;
      instance_capacity = capacity;
    }
    /* Allocate normals only for accepted blades, not rejected mask
     * candidates. Sparse masks can reject most of the requested count. */
    if (placement.terrain && accepted == normals_capacity)
    {
      u32 capacity = normals_capacity ? normals_capacity * 2u : 64u;
      if (capacity < normals_capacity || capacity > count)
        capacity = count;
      Vec3 *grown = realloc(normals, (size_t)capacity * sizeof(*normals));
      if (!grown)
      {
        free(normals);
        free(instances);
        free(partial_ids);
        return false;
      }
      normals = grown;
      normals_capacity = capacity;
    }

    if (out_candidate_ids && index && clip &&
        accepted == partial_capacity)
    {
      u32 capacity = partial_capacity ? partial_capacity * 2u : 64u;
      if (capacity < partial_capacity || capacity > count)
        capacity = count;
      u32 *grown = realloc(partial_ids, (size_t)capacity * sizeof(u32));
      if (!grown)
      {
        free(normals);
        free(instances);
        free(partial_ids);
        return false;
      }
      partial_ids = grown;
      partial_capacity = capacity;
    }
    if (index && !clip)
    {
      index->candidate_slots[candidate_id] = accepted;
      index->slot_candidates[accepted] = candidate_id;
    }
    if (partial_ids) partial_ids[accepted] = candidate_id;

    if (desc->kind != LDK_GRASS_LAYER_VEGETATION)
    {
      float rx = deg_to_rad(desc->rotation_min.x +
          (desc->rotation_max.x - desc->rotation_min.x) *
          s_random_unit(rng));
      float ry = deg_to_rad(desc->rotation_min.y +
          (desc->rotation_max.y - desc->rotation_min.y) *
          s_random_unit(rng));
      float rz = deg_to_rad(desc->rotation_min.z +
          (desc->rotation_max.z - desc->rotation_min.z) *
          s_random_unit(rng));
      Quat pitch = quat_axis_angle(vec3_make(1.0f, 0.0f, 0.0f), rx);
      Quat local_yaw = quat_axis_angle(vec3_make(0.0f, 1.0f, 0.0f), ry);
      Quat roll = quat_axis_angle(vec3_make(0.0f, 0.0f, 1.0f), rz);
      Quat random_rotation = quat_mul(local_yaw, quat_mul(pitch, roll));
      rotation = desc->align_to_normal
          ? quat_mul(align, random_rotation) : random_rotation;
      Vec3 scale = vec3_make(
          desc->scale_min.x + (desc->scale_max.x - desc->scale_min.x) *
              s_random_unit(rng),
          desc->scale_min.y + (desc->scale_max.y - desc->scale_min.y) *
              s_random_unit(rng),
          desc->scale_min.z + (desc->scale_max.z - desc->scale_min.z) *
              s_random_unit(rng));
      if (normals) normals[accepted] = ground_normal;
      instances[accepted++] = mat4_compose(position, rotation, scale);
    }
    else
    {
      if (normals) normals[accepted] = ground_normal;
      instances[accepted++] = mat4_compose(
          position, rotation, vec3_make(1.0f, height, 1.0f));
    }
  }

  if (index) index->ready = true;
  *out_instances = instances;
  *out_normals = normals;
  *out_count = accepted;
  if (out_candidate_ids) *out_candidate_ids = partial_ids;
  else free(partial_ids);
  return true;
}

static LDKGrassPatchEntry *s_patch_get(LDKGrassPatch patch)
{
  LDKGrassPatchEntry *entry;

  if (!s_grass.initialized || patch.version == 0u ||
      patch.index >= s_grass.patch_count)
  {
    return NULL;
  }

  entry = &s_grass.patches[patch.index];
  if (!entry->alive || entry->version != patch.version)
  {
    return NULL;
  }
  return entry;
}

static bool s_batch_instances_reserve(
    LDKGrassBatch *batch, u32 required)
{
  Mat4 *instances;
  u32 capacity;

  if (required <= batch->instance_capacity)
  {
    return true;
  }

  capacity = batch->instance_capacity ? batch->instance_capacity : 256u;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }

  if (!s_allocation_size_is_valid(capacity, sizeof(*instances)))
  {
    return false;
  }

  instances = (Mat4 *)realloc(
      batch->instances, (size_t)capacity * sizeof(*instances));
  if (!instances)
  {
    return false;
  }

  batch->instances = instances;
  if (!s_allocation_size_is_valid(capacity, sizeof(u32)))
  {
    return false;
  }
  u32 *colors = (u32 *)realloc(
      batch->instance_colors, (size_t)capacity * sizeof(*colors));
  if (!colors)
  {
    return false;
  }
  batch->instance_colors = colors;
  batch->instance_capacity = capacity;
  return true;
}

static float s_noise_texel(const LDKImageInfo *image, u32 x, u32 y)
{
  const u8 *pixel = image->pixels +
      ((size_t)y * image->width + x) *
          image->channel_count * image->bytes_per_channel;
  if (image->bytes_per_channel == 2u)
  {
    u16 component;
    memcpy(&component, pixel, sizeof(component));
    return (float)component * (1.0f / 65535.0f);
  }
  return (float)pixel[0] * (1.0f / 255.0f);
}

static float s_noise_sample(float x, float z)
{
  LDKImageInfo image;
  if (!s_grass.color_noise_image ||
      !ldk_image_get_info(s_grass.color_noise_image, &image) ||
      !image.pixels || image.width == 0u || image.height == 0u)
  {
    return 0.5f;
  }
  float scale = s_grass.owner->color_noise_scale;
  float u = x * scale - floorf(x * scale);
  float v = z * scale - floorf(z * scale);
  float fx = u * image.width;
  float fy = v * image.height;
  u32 ix = (u32)floorf(fx);
  u32 iy = (u32)floorf(fy);
  float tx = fx - floorf(fx);
  float ty = fy - floorf(fy);
  tx = tx * tx * (3.0f - 2.0f * tx);
  ty = ty * ty * (3.0f - 2.0f * ty);
  u32 nx = (ix + 1u) % image.width;
  u32 ny = (iy + 1u) % image.height;
  float a = s_noise_texel(&image, ix, iy);
  float b = s_noise_texel(&image, nx, iy);
  float c = s_noise_texel(&image, ix, ny);
  float d = s_noise_texel(&image, nx, ny);
  return (a + (b - a) * tx) +
      ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

static u8 s_noise_color_channel(u8 a, u8 b, float t)
{
  return (u8)((float)a + ((float)b - (float)a) * t + 0.5f);
}

static u32 s_noise_color_lerp(u32 a, u32 b, float t)
{
  t = fminf(fmaxf(t, 0.0f), 1.0f);
  u32 result = 0u;
  for (u32 shift = 0u; shift <= 24u; shift += 8u)
  {
    result |= (u32)s_noise_color_channel(
        (u8)((a >> shift) & 255u), (u8)((b >> shift) & 255u), t)
        << shift;
  }
  return result;
}

static u32 s_noise_color_at(const LDKGrassBatch *batch, float x, float z)
{
  const LDKGrassSystem *system = s_grass.owner;
  float n = s_noise_sample(x, z);
  float stop1 = system->color_stop_1_position;
  float stop2 = system->color_stop_2_position;
  if (stop1 == 0.0f && stop2 == 0.0f)
  {
    stop1 = 1.0f / 3.0f;
    stop2 = 2.0f / 3.0f;
  }
  if (n <= stop1)
  {
    return s_noise_color_lerp(batch->noise_color_stop_0_color,
        batch->noise_color_stop_1_color,
        stop1 > 0.0f
            ? n / stop1 : 0.0f);
  }
  if (n <= stop2)
  {
    return s_noise_color_lerp(batch->noise_color_stop_1_color,
        batch->noise_color_stop_2_color,
        (n - stop1) / (stop2 - stop1));
  }
  return s_noise_color_lerp(batch->noise_color_stop_2_color,
      batch->noise_color_stop_3_color,
      stop2 < 1.0f
          ? (n - stop2) / (1.0f - stop2) : 1.0f);
}

static bool s_batch_rebuild(u32 batch_index)
{
  LDKGrassBatch *batch;
  u64 required = 0u;
  u32 write_index = 0u;

  if (batch_index >= s_grass.batch_count)
  {
    return false;
  }

  batch = &s_grass.batches[batch_index];
  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    const LDKGrassPatchEntry *entry = &s_grass.patches[i];

    if (entry->alive && entry->batch_index == batch_index)
    {
      required += entry->instance_count;
      if (required > UINT32_MAX)
      {
        return false;
      }
    }
  }

  if (!s_batch_instances_reserve(batch, (u32)required))
  {
    return false;
  }

  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[i];

    if (!entry->alive || entry->batch_index != batch_index)
    {
      continue;
    }

    entry->instance_offset = write_index;
    if (entry->instance_count == 0u)
    {
      continue;
    }
    memcpy(batch->instances + write_index, entry->instances,
        (size_t)entry->instance_count * sizeof(*entry->instances));
    for (u32 j = 0u; j < entry->instance_count; ++j)
    {
      const Mat4 *m = &entry->instances[j];
      batch->instance_colors[write_index + j] =
          batch->use_noise_color && s_grass.color_noise_image
              ? s_noise_color_at(batch, m->m[12], m->m[14])
              : 0xffffffffu;
    }
    write_index += entry->instance_count;
  }

  batch->instance_count = write_index;
  if (write_index == 0u)
  {
    if (ldk_renderer_instance_set_is_valid(
            s_grass.renderer, batch->instance_set))
    {
      ldk_renderer_instance_set_destroy(
          s_grass.renderer, batch->instance_set);
    }
    batch->instance_set = ldk_renderer_instance_set_null();
  }
  else if (ldk_renderer_instance_set_is_valid(
               s_grass.renderer, batch->instance_set))
  {
    if (!ldk_renderer_instance_set_update_colored(s_grass.renderer,
            batch->instance_set, batch->instances,
            batch->instance_colors, write_index))
    {
      return false;
    }
  }
  else
  {
    batch->instance_set = ldk_renderer_instance_set_create_colored(
        s_grass.renderer, batch->instances,
        batch->instance_colors, write_index);
    if (!ldk_renderer_instance_set_is_valid(
            s_grass.renderer, batch->instance_set))
    {
      return false;
    }
  }
  batch->lod_dirty = true;
  batch->dirty = false;
  return true;
}

static u8 s_color_channel_lerp(u8 a, u8 b, float t)
{
  float value = (float)a + ((float)b - (float)a) * t;

  value = fminf(fmaxf(value, 0.0f), 255.0f);
  return (u8)(value + 0.5f);
}

static rgba32 s_color_lerp(rgba32 a, rgba32 b, float t)
{
  u8 ar = (u8)((a >> 24u) & 0xffu);
  u8 ag = (u8)((a >> 16u) & 0xffu);
  u8 ab = (u8)((a >> 8u) & 0xffu);
  u8 aa = (u8)(a & 0xffu);
  u8 br = (u8)((b >> 24u) & 0xffu);
  u8 bg = (u8)((b >> 16u) & 0xffu);
  u8 bb = (u8)((b >> 8u) & 0xffu);
  u8 ba = (u8)(b & 0xffu);

  return ((u32)s_color_channel_lerp(ar, br, t) << 24u) |
         ((u32)s_color_channel_lerp(ag, bg, t) << 16u) |
         ((u32)s_color_channel_lerp(ab, bb, t) << 8u) |
         (u32)s_color_channel_lerp(aa, ba, t);
}

static float s_profile_slope(
    const LDKGrassBladeShape *shape, u32 point_index)
{
  u32 first = point_index > 0u ? point_index - 1u : point_index;
  u32 last = point_index + 1u < LDK_GRASS_PROFILE_POINT_COUNT
      ? point_index + 1u
      : point_index;
  float delta_height =
      shape->profile[last].height - shape->profile[first].height;
  float delta_radius =
      (shape->profile[last].width - shape->profile[first].width) * 0.5f;

  return delta_height > 0.0f ? delta_radius / delta_height : 0.0f;
}

static void s_ribbon_mesh_write(const LDKGrassBatch *batch,
    LDKGrassBladeLOD lod, float angle, LDKMeshVertex *vertices,
    u32 *vertex_count, u32 *indices, u32 *index_count)
{
  Vec3 right = vec3_make(cosf(angle), 0.0f, sinf(angle));
  Vec3 front_normal = vec3_make(-sinf(angle), 0.0f, cosf(angle));
  u32 first_vertex = *vertex_count;
  u32 level_count = lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED
      ? 3u : LDK_GRASS_PROFILE_POINT_COUNT;

  for (u32 level = 0u; level < level_count; ++level)
  {
    u32 profile = lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED && level > 0u
        ? level + 1u : level;
    float height = batch->shape.profile[profile].height;
    float half_width = batch->shape.profile[profile].width * 0.5f;
    u32 color = LDK_RGBA32(s_color_lerp(
        batch->bottom_color, batch->top_color, height));
    LDKMeshVertex *left = &vertices[(*vertex_count)++];
    LDKMeshVertex *right_vertex = &vertices[(*vertex_count)++];

    left->position = vec3_add(
        vec3_make(0.0f, height, 0.0f), vec3_mul(right, -half_width));
    left->normal = front_normal;
    left->uv = vec2_make(0.0f, height);
    left->color = color;
    left->tangent = vec4_make(
        front_normal.x, front_normal.y, front_normal.z, 1.0f);

    right_vertex->position = vec3_add(
        vec3_make(0.0f, height, 0.0f), vec3_mul(right, half_width));
    right_vertex->normal = front_normal;
    right_vertex->uv = vec2_make(1.0f, height);
    right_vertex->color = color;
    right_vertex->tangent = vec4_make(
        front_normal.x, front_normal.y, front_normal.z, 1.0f);
  }

  for (u32 level = 0u; level + 1u < level_count; ++level)
  {
    u32 lower_left = first_vertex + level * 2u;
    u32 lower_right = lower_left + 1u;
    u32 upper_left = lower_left + 2u;
    u32 upper_right = lower_left + 3u;

    indices[(*index_count)++] = lower_left;
    indices[(*index_count)++] = lower_right;
    indices[(*index_count)++] = upper_right;
    indices[(*index_count)++] = lower_left;
    indices[(*index_count)++] = upper_right;
    indices[(*index_count)++] = upper_left;
  }
}

static void s_radial_mesh_write(const LDKGrassBatch *batch,
    LDKGrassBladeLOD lod, LDKMeshVertex *vertices,
    u32 *vertex_count, u32 *indices, u32 *index_count)
{
  u32 sides = batch->shape.side_count;
  u32 level_count = lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED
      ? 3u : LDK_GRASS_PROFILE_POINT_COUNT;
  bool pointed =
      batch->shape.profile[LDK_GRASS_PROFILE_POINT_COUNT - 1u].width <=
      STDXM_EPS;
  u32 ring_count = pointed ? level_count - 1u : level_count;

  if (lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED && sides > 3u)
  {
    sides = (sides + 1u) / 2u;
    if (sides < 3u)
    {
      sides = 3u;
    }
  }

  for (u32 level = 0u; level < ring_count; ++level)
  {
    u32 profile = lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED && level > 0u
        ? level + 1u : level;
    float height = batch->shape.profile[profile].height;
    float radius = batch->shape.profile[profile].width * 0.5f;
    float slope = s_profile_slope(&batch->shape, profile);
    u32 color = LDK_RGBA32(s_color_lerp(
        batch->bottom_color, batch->top_color, height));

    for (u32 side = 0u; side < sides; ++side)
    {
      float angle = (float)side * 2.0f * LDK_GRASS_PI / (float)sides;
      float x = cosf(angle);
      float z = sinf(angle);
      LDKMeshVertex *vertex = &vertices[(*vertex_count)++];

      vertex->position = vec3_make(x * radius, height, z * radius);
      vertex->normal = vec3_norm(vec3_make(x, -slope, z));
      vertex->uv = vec2_make((float)side / (float)sides, height);
      vertex->color = color;
      vertex->tangent = vec4_make(0.0f, 0.0f, 1.0f, 1.0f);
    }
  }

  for (u32 level = 0u; level + 1u < ring_count; ++level)
  {
    u32 lower = level * sides;
    u32 upper = (level + 1u) * sides;

    for (u32 side = 0u; side < sides; ++side)
    {
      u32 next = (side + 1u) % sides;

      indices[(*index_count)++] = lower + side;
      indices[(*index_count)++] = upper + next;
      indices[(*index_count)++] = lower + next;
      indices[(*index_count)++] = lower + side;
      indices[(*index_count)++] = upper + side;
      indices[(*index_count)++] = upper + next;
    }
  }

  if (pointed)
  {
    const LDKGrassProfilePoint *tip =
        &batch->shape.profile[LDK_GRASS_PROFILE_POINT_COUNT - 1u];
    u32 apex = (*vertex_count)++;
    u32 last_ring = (ring_count - 1u) * sides;

    vertices[apex].position = vec3_make(0.0f, tip->height, 0.0f);
    vertices[apex].normal = vec3_make(0.0f, 1.0f, 0.0f);
    vertices[apex].uv = vec2_make(0.5f, 1.0f);
    vertices[apex].color = LDK_RGBA32(batch->top_color);
    vertices[apex].tangent = vec4_make(0.0f, 0.0f, 1.0f, 1.0f);

    for (u32 side = 0u; side < sides; ++side)
    {
      u32 next = (side + 1u) % sides;

      indices[(*index_count)++] = last_ring + side;
      indices[(*index_count)++] = apex;
      indices[(*index_count)++] = last_ring + next;
    }
  }
  else
  {
    u32 cap_ring = *vertex_count;
    u32 source_ring = (ring_count - 1u) * sides;
    u32 center;

    for (u32 side = 0u; side < sides; ++side)
    {
      vertices[*vertex_count] = vertices[source_ring + side];
      vertices[*vertex_count].normal = vec3_make(0.0f, 1.0f, 0.0f);
      vertices[*vertex_count].tangent.w = 0.0f;
      *vertex_count += 1u;
    }

    center = (*vertex_count)++;
    vertices[center].position = vec3_make(0.0f, 1.0f, 0.0f);
    vertices[center].normal = vec3_make(0.0f, 1.0f, 0.0f);
    vertices[center].uv = vec2_make(0.5f, 0.5f);
    vertices[center].color = LDK_RGBA32(batch->top_color);
    vertices[center].tangent = vec4_make(0.0f, 0.0f, 1.0f, 0.0f);

    for (u32 side = 0u; side < sides; ++side)
    {
      u32 next = (side + 1u) % sides;

      indices[(*index_count)++] = center;
      indices[(*index_count)++] = cap_ring + next;
      indices[(*index_count)++] = cap_ring + side;
    }
  }
}

static float s_profile_width_at_height(
    const LDKGrassBladeShape *shape, float height)
{
  for (u32 level = 0u;
       level + 1u < LDK_GRASS_PROFILE_POINT_COUNT; ++level)
  {
    const LDKGrassProfilePoint *lower = &shape->profile[level];
    const LDKGrassProfilePoint *upper = &shape->profile[level + 1u];

    if (height <= upper->height)
    {
      float t = (height - lower->height) /
          (upper->height - lower->height);
      return lower->width + (upper->width - lower->width) * t;
    }
  }
  return shape->profile[LDK_GRASS_PROFILE_POINT_COUNT - 1u].width;
}

static Vec3 s_thorny_stem_center(float height, float base_width)
{
  float x = (sinf(height * 5.1f) * 0.72f + height * 0.38f) *
      base_width;
  float z = (sinf(height * 7.3f + 0.65f) - sinf(0.65f)) * 0.48f *
      base_width;

  return vec3_make(x, height, z);
}

static void s_thorny_stem_mesh_write(const LDKGrassBatch *batch,
    LDKGrassBladeLOD lod, LDKMeshVertex *vertices,
    u32 *vertex_count, u32 *indices, u32 *index_count)
{
  const LDKGrassBladeShape *shape = &batch->shape;
  float base_width = shape->profile[0].width;
  u32 sides = shape->side_count;
  u32 level_count = lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED
      ? 3u : LDK_GRASS_PROFILE_POINT_COUNT;
  bool pointed =
      shape->profile[LDK_GRASS_PROFILE_POINT_COUNT - 1u].width <=
      STDXM_EPS;
  u32 ring_count = pointed ? level_count - 1u : level_count;
  u32 stem_first = *vertex_count;

  if (lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED && sides > 3u)
  {
    sides = (sides + 1u) / 2u;
    if (sides < 3u)
    {
      sides = 3u;
    }
  }

  for (u32 level = 0u; level < ring_count; ++level)
  {
    u32 profile = lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED && level > 0u
        ? level + 1u : level;
    float height = shape->profile[profile].height;
    float radius = shape->profile[profile].width * 0.5f;
    Vec3 center = s_thorny_stem_center(height, base_width);
    u32 color = LDK_RGBA32(s_color_lerp(
        batch->bottom_color, batch->top_color, height));

    for (u32 side = 0u; side < sides; ++side)
    {
      float angle = (float)side * 2.0f * LDK_GRASS_PI / (float)sides;
      Vec3 normal = vec3_make(cosf(angle), 0.0f, sinf(angle));
      LDKMeshVertex *vertex = &vertices[(*vertex_count)++];

      vertex->position = vec3_add(center, vec3_mul(normal, radius));
      vertex->normal = normal;
      vertex->uv = vec2_make((float)side / (float)sides, height);
      vertex->color = color;
      vertex->tangent = vec4_make(0.0f, 0.0f, 1.0f, 1.0f);
    }
  }

  for (u32 level = 0u; level + 1u < ring_count; ++level)
  {
    u32 lower = stem_first + level * sides;
    u32 upper = lower + sides;

    for (u32 side = 0u; side < sides; ++side)
    {
      u32 next = (side + 1u) % sides;

      indices[(*index_count)++] = lower + side;
      indices[(*index_count)++] = upper + next;
      indices[(*index_count)++] = lower + next;
      indices[(*index_count)++] = lower + side;
      indices[(*index_count)++] = upper + side;
      indices[(*index_count)++] = upper + next;
    }
  }

  if (pointed)
  {
    u32 apex = (*vertex_count)++;
    u32 last_ring = stem_first + (ring_count - 1u) * sides;
    Vec3 tip = s_thorny_stem_center(1.0f, base_width);

    vertices[apex].position = tip;
    vertices[apex].normal = vec3_make(0.0f, 1.0f, 0.0f);
    vertices[apex].uv = vec2_make(0.5f, 1.0f);
    vertices[apex].color = LDK_RGBA32(batch->top_color);
    vertices[apex].tangent = vec4_make(0.0f, 0.0f, 1.0f, 1.0f);
    for (u32 side = 0u; side < sides; ++side)
    {
      u32 next = (side + 1u) % sides;

      indices[(*index_count)++] = last_ring + side;
      indices[(*index_count)++] = apex;
      indices[(*index_count)++] = last_ring + next;
    }
  }
  else
  {
    u32 cap_ring = *vertex_count;
    u32 source_ring = stem_first + (ring_count - 1u) * sides;
    u32 center;

    for (u32 side = 0u; side < sides; ++side)
    {
      vertices[*vertex_count] = vertices[source_ring + side];
      vertices[*vertex_count].normal = vec3_make(0.0f, 1.0f, 0.0f);
      vertices[*vertex_count].tangent.w = 0.0f;
      *vertex_count += 1u;
    }
    center = (*vertex_count)++;
    vertices[center] = vertices[cap_ring];
    vertices[center].position = s_thorny_stem_center(1.0f, base_width);
    vertices[center].uv = vec2_make(0.5f, 1.0f);
    for (u32 side = 0u; side < sides; ++side)
    {
      u32 next = (side + 1u) % sides;

      indices[(*index_count)++] = center;
      indices[(*index_count)++] = cap_ring + next;
      indices[(*index_count)++] = cap_ring + side;
    }
  }

  /* Distant thorny grass keeps only its simplified stem. Individual spikes
   * become disconnected screen-space fragments and add no readable detail. */
  if (lod != LDK_GRASS_BLADE_LOD_FULL)
  {
    return;
  }

  for (u32 thorn = 0u; thorn < LDK_GRASS_THORN_COUNT; ++thorn)
  {
    float thorn_t = (float)thorn / (float)(LDK_GRASS_THORN_COUNT - 1u);
    float height = 0.16f + thorn_t * 0.68f;
    float angle = (float)thorn * 2.39996323f + 0.35f;
    Vec3 outward = vec3_make(cosf(angle), 0.0f, sinf(angle));
    Vec3 around = vec3_make(-outward.z, 0.0f, outward.x);
    float stem_radius = s_profile_width_at_height(shape, height) * 0.5f;
    float thorn_length = base_width *
        (LDK_GRASS_THORN_MAX_LENGTH_SCALE - thorn_t * 0.22f);
    float base_radius = base_width * (0.105f - thorn_t * 0.025f);
    Vec3 attachment = vec3_add(
        s_thorny_stem_center(height, base_width),
        vec3_mul(outward, stem_radius));
    Vec3 tip = vec3_add(
        vec3_add(attachment, vec3_mul(outward, thorn_length)),
        vec3_make(0.0f, thorn_length * 0.12f, 0.0f));
    u32 first = *vertex_count;
    u32 color = LDK_RGBA32(s_color_lerp(
        batch->bottom_color, batch->top_color, height));
    Vec3 base[3];

    base[0] = vec3_add(attachment, vec3_make(0.0f, base_radius, 0.0f));
    base[1] = vec3_add(vec3_add(attachment,
                               vec3_make(0.0f, -base_radius * 0.65f, 0.0f)),
        vec3_mul(around, base_radius * 0.9f));
    base[2] = vec3_add(vec3_add(attachment,
                               vec3_make(0.0f, -base_radius * 0.65f, 0.0f)),
        vec3_mul(around, -base_radius * 0.9f));
    for (u32 corner = 0u; corner < 3u; ++corner)
    {
      vertices[*vertex_count].position = base[corner];
      vertices[*vertex_count].normal = outward;
      vertices[*vertex_count].uv = vec2_make((float)corner * 0.5f, height);
      vertices[*vertex_count].color = color;
      vertices[*vertex_count].tangent =
          vec4_make(0.0f, 0.0f, 1.0f, 0.0f);
      *vertex_count += 1u;
    }
    vertices[*vertex_count].position = tip;
    vertices[*vertex_count].normal = outward;
    vertices[*vertex_count].uv = vec2_make(0.5f, height);
    vertices[*vertex_count].color = color;
    vertices[*vertex_count].tangent =
        vec4_make(0.0f, 0.0f, 1.0f, 0.0f);
    *vertex_count += 1u;

    indices[(*index_count)++] = first;
    indices[(*index_count)++] = first + 1u;
    indices[(*index_count)++] = first + 3u;
    indices[(*index_count)++] = first + 1u;
    indices[(*index_count)++] = first + 2u;
    indices[(*index_count)++] = first + 3u;
    indices[(*index_count)++] = first + 2u;
    indices[(*index_count)++] = first;
    indices[(*index_count)++] = first + 3u;
  }
}

static void s_plumed_stem_mesh_write(const LDKGrassBatch *batch,
    LDKGrassBladeLOD lod, LDKMeshVertex *vertices,
    u32 *vertex_count, u32 *indices, u32 *index_count)
{
  static const struct
  {
    float base_height;
    float tip_height;
    float reach;
    float width_scale;
  } plumes[LDK_GRASS_PLUME_COUNT] = {
      {0.73f, 1.00f, 2.25f, 0.95f},
      {0.67f, 0.84f, 4.00f, 0.85f},
      {0.64f, 0.74f, 3.00f, 0.70f},
  };
  const float width = batch->shape.profile[0].width;
  const Vec3 normal = vec3_make(0.0f, 0.0f, 1.0f);
  const Vec4 tangent = vec4_make(1.0f, 0.0f, 0.0f, 1.0f);
  u32 first = *vertex_count;
  u32 stem_levels = lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED
      ? 3u : LDK_GRASS_PROFILE_POINT_COUNT;
  u32 plume_segments = lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED
      ? 4u : LDK_GRASS_PLUME_SEGMENT_COUNT;

  /* A narrow ribbon forms the entire stem. */
  for (u32 level = 0u; level < stem_levels; ++level)
  {
    u32 profile = lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED && level > 0u
        ? level + 1u : level;
    float t = (float)profile / (float)(LDK_GRASS_PROFILE_POINT_COUNT - 1u);
    float x = width * 0.13f * t * t;
    float y = 0.75f * t;
    float half_width = width * (0.07f - 0.02f * t);
    u32 color = LDK_RGBA32(s_color_lerp(
        batch->bottom_color, batch->top_color, t * 0.20f));

    for (u32 side = 0u; side < 2u; ++side)
    {
      LDKMeshVertex *vertex = &vertices[(*vertex_count)++];
      vertex->position = vec3_make(
          x + (side == 0u ? -half_width : half_width), y, 0.0f);
      vertex->normal = normal;
      vertex->uv = vec2_make((float)side, y);
      vertex->color = color;
      vertex->tangent = tangent;
    }
  }
  for (u32 level = 0u; level + 1u < stem_levels; ++level)
  {
    u32 lower = first + level * 2u;
    indices[(*index_count)++] = lower;
    indices[(*index_count)++] = lower + 1u;
    indices[(*index_count)++] = lower + 3u;
    indices[(*index_count)++] = lower;
    indices[(*index_count)++] = lower + 3u;
    indices[(*index_count)++] = lower + 2u;
  }

  /* Three tapered ribbons grow from the upper portion of the same stem. */
  for (u32 plume = 0u; plume < LDK_GRASS_PLUME_COUNT; ++plume)
  {
    float stem_t = plumes[plume].base_height / 0.75f;
    float base_x = width * 0.13f * stem_t * stem_t;
    float reach = width * plumes[plume].reach;
    float rise = plumes[plume].tip_height - plumes[plume].base_height;

    first = *vertex_count;
    for (u32 level = 0u; level <= plume_segments; ++level)
    {
      float t = (float)level / (float)plume_segments;
      float x = base_x + reach * (0.18f * t + 0.82f * t * t);
      float y = plumes[plume].base_height + rise * t;
      float dx = reach * (0.18f + 1.64f * t);
      float dy = rise;
      float inv_length = 1.0f / sqrtf(dx * dx + dy * dy);
      float half_width = width * plumes[plume].width_scale * 0.5f *
          fmaxf(0.0f, sinf(t * LDK_GRASS_PI)) * (1.0f - t * 0.18f);
      float edge_x = dy * inv_length;
      float edge_y = -dx * inv_length;
      float left_width = half_width * (level % 3u == 1u ? 0.76f : 1.0f);
      float right_width = half_width * (level % 4u == 2u ? 0.82f : 1.0f);
      u32 color = LDK_RGBA32(s_color_lerp(batch->bottom_color,
          batch->top_color, 0.80f + 0.20f * t));
      LDKMeshVertex *left = &vertices[(*vertex_count)++];
      LDKMeshVertex *right = &vertices[(*vertex_count)++];

      left->position = vec3_make(
          x - edge_x * left_width, y - edge_y * left_width, 0.0f);
      left->normal = normal;
      left->uv = vec2_make(0.0f, y);
      left->color = color;
      left->tangent = tangent;

      right->position = vec3_make(
          x + edge_x * right_width, y + edge_y * right_width, 0.0f);
      right->normal = normal;
      right->uv = vec2_make(1.0f, y);
      right->color = color;
      right->tangent = tangent;
    }

    for (u32 level = 0u; level < plume_segments; ++level)
    {
      u32 lower = first + level * 2u;
      indices[(*index_count)++] = lower;
      indices[(*index_count)++] = lower + 1u;
      indices[(*index_count)++] = lower + 3u;
      indices[(*index_count)++] = lower;
      indices[(*index_count)++] = lower + 3u;
      indices[(*index_count)++] = lower + 2u;
    }
  }
}

static bool s_batch_mesh_create(LDKGrassBatch *batch, LDKGrassBladeLOD lod)
{
  LDKMeshVertex vertices[LDK_GRASS_MAX_MESH_VERTEX_COUNT];
  u32 indices[LDK_GRASS_MAX_MESH_INDEX_COUNT];
  LDKRendererMeshDesc desc = {0};
  u32 vertex_count = 0u;
  u32 index_count = 0u;

  if (!batch || !s_shape_is_valid(&batch->shape) || !s_grass.renderer ||
      (lod != LDK_GRASS_BLADE_LOD_FULL &&
          lod != LDK_GRASS_BLADE_LOD_SIMPLIFIED))
  {
    return false;
  }

  if (batch->kind != LDK_GRASS_LAYER_VEGETATION)
  {
    if (lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED)
    {
      /* Use the same imported mesh for LOD 1. The existing GrassSystem
       * instance thinning still applies; no procedural geometry is built. */
      if (!ldk_renderer_mesh_is_valid(s_grass.renderer, batch->mesh))
        return false;
      batch->lod_mesh = batch->mesh;
      batch->lod_index_count = batch->index_count;
      return true;
    }
    const LDKMeshData *data = s_decoration_mesh_get(
        &batch->mesh_path, batch->mesh_index);
    if (!data || !data->vertices || !data->indices ||
        data->vertex_count == 0u || data->index_count == 0u)
      return false;
    desc.vertices = data->vertices;
    desc.vertex_count = data->vertex_count;
    desc.indices = data->indices;
    desc.index_count = data->index_count;
    desc.has_tangents = data->has_tangents;
    /* Validate the authored submesh/material layout before creating GPU
     * resources. A malformed or unsupported mesh must not leave a half-built
     * batch that would be retried unsuccessfully on every frame. */
    LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    if (!assets) return false;
    LDKAssetMesh asset = ldk_asset_manager_mesh_load_shared(
        assets, batch->mesh_path.buf, NULL);
    if (x_handle_is_null(asset.h)) return false;
    u32 slots = ldk_asset_manager_mesh_material_slot_count(
        assets, asset, batch->mesh_index);
    if (slots > LDK_GRASS_MESH_MATERIAL_SLOT_COUNT)
    {
      ldk_log_error("Grass mesh %s has %u material slots (maximum %u).\n",
          batch->mesh_path.buf, slots,
          LDK_GRASS_MESH_MATERIAL_SLOT_COUNT);
      return false;
    }
    if (!slots) slots = 1u;
    u32 count = ldk_asset_manager_mesh_submesh_count(
        assets, asset, batch->mesh_index);
    u32 submesh_count = count ? count : 1u;
    LDKMeshSubmesh *submeshes = calloc(submesh_count, sizeof(*submeshes));
    if (!submeshes) return false;
    for (u32 i = 0u; i < submesh_count; ++i)
    {
      LDKMeshSubmesh submesh = {0u, desc.index_count, 0u};
      if (count)
      {
        const LDKMeshSubmesh *source =
            ldk_asset_manager_mesh_submesh_at(
                assets, asset, batch->mesh_index, i);
        if (!source)
        {
          free(submeshes);
          return false;
        }
        submesh = *source;
      }
      if (submesh.material_slot >= slots ||
          submesh.first_index > desc.index_count ||
          submesh.index_count > desc.index_count - submesh.first_index)
      {
        free(submeshes);
        return false;
      }
      submeshes[i] = submesh;
    }
    LDKResourceMesh mesh = ldk_renderer_mesh_create(s_grass.renderer, &desc);
    if (!ldk_renderer_mesh_is_valid(s_grass.renderer, mesh))
    {
      free(submeshes);
      return false;
    }
    batch->mesh = mesh;
    batch->index_count = desc.index_count;
    batch->material_slot_count = slots;
    batch->submesh_count = submesh_count;
    batch->submeshes = submeshes;
    float minimum = FLT_MAX, maximum = -FLT_MAX;
    for (u32 i = 0u; i < data->vertex_count; ++i)
    {
      const float y = data->vertices[i].position.y;
      if (y < minimum) minimum = y;
      if (y > maximum) maximum = y;
    }
    batch->mesh_min_y = minimum;
    batch->mesh_inv_height = maximum > minimum ?
        1.0f / (maximum - minimum) : 0.0f;
    return true;
  }

  if (batch->shape.topology == LDK_GRASS_BLADE_TOPOLOGY_CUSTOM)
  {
    const u32 slot = batch->shape.custom_slot;
    LDKGrassBladeMeshGenerator generator = s_custom_generators[slot];
    LDKMeshData custom_mesh = {0};
    float max_extent = 0.0f;
    bool valid = generator != NULL;

    if (!generator)
    {
      if (!s_custom_error_reported[slot])
      {
        ldk_log_error("No grass blade generator registered for CUSTOM_%u.\n",
            slot);
        s_custom_error_reported[slot] = true;
      }
      return false;
    }

    if (!generator(&batch->shape, batch->bottom_color, batch->top_color,
            lod, &custom_mesh))
    {
      ldk_mesh_data_destroy(&custom_mesh);
      if (!s_custom_error_reported[slot])
      {
        ldk_log_error("Grass blade generator CUSTOM_%u failed.\n", slot);
        s_custom_error_reported[slot] = true;
      }
      return false;
    }

    for (u32 i = 0u; i < LDK_GRASS_PROFILE_POINT_COUNT; ++i)
    {
      max_extent = float_max(max_extent,
          batch->shape.profile[i].width * 0.5f);
    }

    valid = custom_mesh.vertices && custom_mesh.indices &&
        custom_mesh.vertex_count > 0u &&
        custom_mesh.index_count > 0u &&
        custom_mesh.index_count % 3u == 0u;
    for (u32 i = 0u; valid && i < custom_mesh.vertex_count; ++i)
    {
      const LDKMeshVertex *vertex = &custom_mesh.vertices[i];
      valid = s_vec3_is_finite(vertex->position) &&
          s_vec3_is_finite(vertex->normal) &&
          isfinite(vertex->uv.x) && isfinite(vertex->uv.y) &&
          vertex->position.x >= -max_extent &&
          vertex->position.x <= max_extent &&
          vertex->position.z >= -max_extent &&
          vertex->position.z <= max_extent &&
          vertex->position.y >= 0.0f && vertex->position.y <= 1.0f &&
          vertex->uv.y >= 0.0f && vertex->uv.y <= 1.0f;
      if (custom_mesh.has_tangents)
      {
        valid = valid && isfinite(vertex->tangent.x) &&
            isfinite(vertex->tangent.y) &&
            isfinite(vertex->tangent.z) &&
            isfinite(vertex->tangent.w);
      }
    }
    for (u32 i = 0u; valid && i < custom_mesh.index_count; ++i)
    {
      valid = custom_mesh.indices[i] < custom_mesh.vertex_count;
    }

    if (!valid)
    {
      if (!s_custom_error_reported[slot])
      {
        ldk_log_error("Grass blade generator CUSTOM_%u returned invalid "
            "geometry.\n", slot);
        s_custom_error_reported[slot] = true;
      }
      ldk_mesh_data_destroy(&custom_mesh);
      return false;
    }

    desc.vertices = custom_mesh.vertices;
    desc.vertex_count = custom_mesh.vertex_count;
    desc.indices = custom_mesh.indices;
    desc.index_count = custom_mesh.index_count;
    desc.has_tangents = custom_mesh.has_tangents;
    LDKResourceMesh mesh = ldk_renderer_mesh_create(s_grass.renderer, &desc);
    ldk_mesh_data_destroy(&custom_mesh);
    if (!ldk_renderer_mesh_is_valid(s_grass.renderer, mesh))
    {
      return false;
    }
    if (lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED)
    {
      batch->lod_mesh = mesh;
      batch->lod_index_count = desc.index_count;
    }
    else
    {
      batch->mesh = mesh;
      batch->index_count = desc.index_count;
    }
    return true;
  }

  memset(vertices, 0, sizeof(vertices));
  switch (batch->shape.topology)
  {
  case LDK_GRASS_BLADE_TOPOLOGY_RIBBON:
    s_ribbon_mesh_write(batch, lod, 0.0f, vertices, &vertex_count,
        indices, &index_count);
    break;
  case LDK_GRASS_BLADE_TOPOLOGY_CROSSED_RIBBONS:
    s_ribbon_mesh_write(batch, lod, 0.0f, vertices, &vertex_count,
        indices, &index_count);
    s_ribbon_mesh_write(batch, lod, LDK_GRASS_PI * 0.5f, vertices,
        &vertex_count, indices, &index_count);
    break;
  case LDK_GRASS_BLADE_TOPOLOGY_TRIPLE_RIBBONS:
    s_ribbon_mesh_write(batch, lod, 0.0f, vertices, &vertex_count,
        indices, &index_count);
    s_ribbon_mesh_write(batch, lod, LDK_GRASS_PI / 3.0f, vertices,
        &vertex_count, indices, &index_count);
    s_ribbon_mesh_write(batch, lod, LDK_GRASS_PI * 2.0f / 3.0f, vertices,
        &vertex_count, indices, &index_count);
    break;
  case LDK_GRASS_BLADE_TOPOLOGY_RADIAL:
    s_radial_mesh_write(
        batch, lod, vertices, &vertex_count, indices, &index_count);
    break;
  case LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM:
    s_thorny_stem_mesh_write(
        batch, lod, vertices, &vertex_count, indices, &index_count);
    break;
  case LDK_GRASS_BLADE_TOPOLOGY_PLUMED_STEM:
    s_plumed_stem_mesh_write(
        batch, lod, vertices, &vertex_count, indices, &index_count);
    break;
  default:
    return false;
  }

  desc.vertices = vertices;
  desc.vertex_count = vertex_count;
  desc.indices = indices;
  desc.index_count = index_count;
  desc.has_tangents = true;
  LDKResourceMesh mesh = ldk_renderer_mesh_create(s_grass.renderer, &desc);
  if (!ldk_renderer_mesh_is_valid(s_grass.renderer, mesh))
  {
    return false;
  }

  if (lod == LDK_GRASS_BLADE_LOD_SIMPLIFIED)
  {
    batch->lod_mesh = mesh;
    batch->lod_index_count = index_count;
  }
  else
  {
    batch->mesh = mesh;
    batch->index_count = index_count;
  }
  return true;
}

static bool s_batch_material_create(LDKGrassBatch *batch)
{
  if (!batch || !s_grass.renderer) return false;
  if (batch->kind != LDK_GRASS_LAYER_VEGETATION)
  {
    LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    if (!assets || !batch->material_slot_count) return false;
    LDKMaterialIOContext context = {0};
    context.assets = assets;
    for (u32 slot = 0u; slot < batch->material_slot_count; ++slot)
    {
      LDKMaterialDesc desc;
      if (!ldk_material_desc_defaults(LDK_MATERIAL_TYPE_VERTEX_COLOR,
              &desc)) return false;
      if (batch->material_paths[slot].length)
      {
        LDKMaterialIOResult result = {0};
        LDKAssetMaterial asset = ldk_asset_manager_material_load_shared(
            &context, batch->material_paths[slot].buf, &result);
        const LDKAssetMaterialData *data = !x_handle_is_null(asset.h)
            ? ldk_asset_manager_material_get_const(assets, asset) : NULL;
        if (!data)
        {
          ldk_log_error("Grass material unavailable: %s\n",
              batch->material_paths[slot].buf);
          return false;
        }
        desc = data->descriptor;
      }
      bool ok = batch->kind == LDK_GRASS_LAYER_TREE
          ? ldk_renderer_tree_material_resolve(s_grass.renderer, assets, &desc,
              batch->wind_strength, batch->wind_speed, batch->wind_direction,
              batch->mesh_min_y, batch->mesh_inv_height,
              &batch->slot_materials[slot], &batch->slot_textures[slot],
              &batch->slot_normal_maps[slot],
              &batch->slot_specular_maps[slot])
          : ldk_renderer_material_resolve(s_grass.renderer, assets, &desc,
              &batch->slot_materials[slot], &batch->slot_textures[slot],
              &batch->slot_normal_maps[slot],
              &batch->slot_specular_maps[slot]);
      if (!ok) return false;
    }
    batch->material = batch->slot_materials[0];
    return true;
  }

  LDKRendererMaterialDesc desc = {0};
  desc.type = batch->unlit ? LDK_MATERIAL_TYPE_VERTEX_COLOR_UNLIT
                           : LDK_MATERIAL_TYPE_VERTEX_COLOR;
  desc.texture = ldk_renderer_texture_null();
  desc.normal_map = ldk_renderer_texture_null();
  desc.specular_map = ldk_renderer_texture_null();
  desc.color = 0xffffffffu;
  desc.alpha_mode = LDK_MATERIAL_ALPHA_MODE_OPAQUE;
  desc.alpha_cutoff = 0.5f;
  desc.specular = batch->specular;
  desc.shininess = batch->shininess;
  desc.emission = batch->emission;
  desc.vegetation_curvature = batch->curvature;
  desc.vegetation_wind_strength = batch->wind_strength;
  desc.vegetation_wind_speed = batch->wind_speed;
  desc.vegetation_wind_direction = batch->wind_direction;
  desc.vegetation_interaction_recovery_time =
      batch->interaction_recovery_time;
  desc.vegetation = true;
  batch->material = ldk_renderer_material_create(s_grass.renderer, &desc);
  return ldk_renderer_material_is_valid(s_grass.renderer, batch->material);
}

static void s_hash_bytes(u64 *hash, const void *data, size_t size)
{
  const u8 *bytes = (const u8 *)data;

  for (size_t i = 0u; i < size; ++i)
  {
    *hash ^= (u64)bytes[i];
    *hash *= UINT64_C(1099511628211);
  }
}

static u64 s_system_config_hash(const LDKGrassSystem *system)
{
  u64 hash = UINT64_C(14695981039346656037);

#define LDK_GRASS_HASH_FIELD(field)                                           \
  s_hash_bytes(&hash, &system->field, sizeof(system->field))

  LDK_GRASS_HASH_FIELD(seed);
  LDK_GRASS_HASH_FIELD(min_tilt_degrees);
  LDK_GRASS_HASH_FIELD(max_tilt_degrees);
  LDK_GRASS_HASH_FIELD(tilt_direction_degrees);
  LDK_GRASS_HASH_FIELD(wind_direction_degrees);
  LDK_GRASS_HASH_FIELD(wind_force);
  LDK_GRASS_HASH_FIELD(wind_speed);
  LDK_GRASS_HASH_FIELD(specular);
  LDK_GRASS_HASH_FIELD(shininess);
  LDK_GRASS_HASH_FIELD(emission);
  LDK_GRASS_HASH_FIELD(casts_shadows);
  LDK_GRASS_HASH_FIELD(unlit);
  LDK_GRASS_HASH_FIELD(color_noise_scale);
  LDK_GRASS_HASH_FIELD(color_stop_1_position);
  LDK_GRASS_HASH_FIELD(color_stop_2_position);
  LDK_GRASS_HASH_FIELD(color_stop_0_color);
  LDK_GRASS_HASH_FIELD(color_stop_1_color);
  LDK_GRASS_HASH_FIELD(color_stop_2_color);
  LDK_GRASS_HASH_FIELD(color_stop_3_color);
  LDK_GRASS_HASH_FIELD(color_noise_texture);
  {
    LDKAssetManager *assets = (LDKAssetManager *)ldk_module_get(
        LDK_MODULE_ASSET_MANAGER);
    LDKAssetHandle handle = {system->color_noise_texture.h};
    const LDKAssetInfo *info = assets &&
        !x_handle_is_null(system->color_noise_texture.h)
        ? ldk_asset_get_info_const(assets, handle) : NULL;
    u64 revision = info ? info->source_revision : 0u;
    s_hash_bytes(&hash, &revision, sizeof(revision));
  }

#undef LDK_GRASS_HASH_FIELD

  s_hash_bytes(&hash, s_custom_generations, sizeof(s_custom_generations));

  return hash;
}

static void s_batch_release(LDKGrassBatch *batch)
{
  if (!batch)
  {
    return;
  }

  if (s_grass.renderer &&
      ldk_renderer_mesh_is_valid(s_grass.renderer, batch->mesh))
  {
    ldk_renderer_mesh_destroy(s_grass.renderer, batch->mesh);
  }
  if (s_grass.renderer && batch->lod_mesh.id != batch->mesh.id &&
      ldk_renderer_mesh_is_valid(s_grass.renderer, batch->lod_mesh))
  {
    ldk_renderer_mesh_destroy(s_grass.renderer, batch->lod_mesh);
  }
  if (s_grass.renderer && batch->kind != LDK_GRASS_LAYER_VEGETATION)
  {
    for (u32 i = 0u; i < LDK_GRASS_MESH_MATERIAL_SLOT_COUNT; ++i)
    {
      ldk_renderer_material_destroy(s_grass.renderer,
          batch->slot_materials[i]);
      ldk_renderer_image_release(s_grass.renderer,
          batch->slot_textures[i]);
      ldk_renderer_image_release(s_grass.renderer,
          batch->slot_normal_maps[i]);
      ldk_renderer_image_release(s_grass.renderer,
          batch->slot_specular_maps[i]);
    }
  }
  else if (s_grass.renderer)
  {
    ldk_renderer_material_destroy(s_grass.renderer, batch->material);
  }
  if (s_grass.renderer &&
      ldk_renderer_instance_set_is_valid(s_grass.renderer, batch->instance_set))
  {
    ldk_renderer_instance_set_destroy(s_grass.renderer, batch->instance_set);
  }
  if (s_grass.renderer &&
      ldk_renderer_instance_set_is_valid(
          s_grass.renderer, batch->lod_instance_set))
  {
    ldk_renderer_instance_set_destroy(s_grass.renderer,
        batch->lod_instance_set);
  }

  free(batch->submeshes);
  free(batch->instances);
  free(batch->instance_colors);
  free(batch->lod_instances);
  free(batch->lod_instance_colors);
  memset(batch, 0, sizeof(*batch));
}

static void s_batches_release_all(void)
{
  for (u32 i = 0u; i < s_grass.batch_count; ++i)
  {
    s_batch_release(&s_grass.batches[i]);
  }
  s_grass.batch_count = 0u;
}

typedef struct LDKGrassPatchRebuild
{
  LDKGrassResolvedDesc resolved;
  Mat4 *instances;
  Vec3 *terrain_normals;
  LDKGrassTerrainCandidateIndex *candidate_index;
  u32 instance_count;
  Vec3 bounds_min;
  Vec3 bounds_max;
  bool has_type;
} LDKGrassPatchRebuild;

static void s_color_noise_defaults(LDKGrassSystem *system)
{
  if (!system)
  {
    return;
  }
  if (system->color_noise_scale == 0.0f)
  {
    system->color_noise_scale = 0.02f;
  }
  if (system->color_stop_1_position == 0.0f &&
      system->color_stop_2_position == 0.0f)
  {
    system->color_stop_1_position = 0.30f;
    system->color_stop_2_position = 0.72f;
  }
  if (system->color_stop_0_color == 0u &&
      system->color_stop_1_color == 0u &&
      system->color_stop_2_color == 0u &&
      system->color_stop_3_color == 0u)
  {
    system->color_stop_0_color = 0xcac279ffu;
    system->color_stop_1_color = 0xa6ba6affu;
    system->color_stop_2_color = 0x70ab56ffu;
    system->color_stop_3_color = 0x427d48ffu;
  }

}

static LDKImage *s_color_noise_resolve(const LDKGrassSystem *system)
{
  LDKAssetManager *assets = (LDKAssetManager *)ldk_module_get(
      LDK_MODULE_ASSET_MANAGER);
  const LDKAssetImageData *data;
  if (!assets || x_handle_is_null(system->color_noise_texture.h))
  {
    return NULL;
  }
  data = ldk_asset_manager_image_get_const(
      assets, system->color_noise_texture);
  return data && !data->is_missing ? data->image : NULL;
}

static bool s_config_rebuild(LDKGrassSystem *system)
{
  s_color_noise_defaults(system);
  LDKGrassPatchRebuild *rebuilt = NULL;
  LDKImage *old_image = s_grass.color_noise_image;
  LDKImage *new_image = s_color_noise_resolve(system);
  if (!x_handle_is_null(system->color_noise_texture.h) && !new_image)
  {
    return false;
  }
  s_grass.color_noise_image = new_image;

  if (!system || !s_system_config_is_valid(system) ||
      !s_batches_reserve(LDK_TERRAIN_VEGETATION_LAYER_COUNT))
  {
    s_grass.color_noise_image = old_image;
    
    return false;
  }

  if (s_grass.patch_count > 0u)
  {
    if (!s_allocation_size_is_valid(
            s_grass.patch_count, sizeof(*rebuilt)))
    {
      s_grass.color_noise_image = old_image;
      
      return false;
    }
    rebuilt = (LDKGrassPatchRebuild *)calloc(
        s_grass.patch_count, sizeof(*rebuilt));
    if (!rebuilt)
    {
      s_grass.color_noise_image = old_image;
      
      return false;
    }
  }

  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[i];
    LDKGrassPatchRebuild *item = &rebuilt[i];

    if (!entry->alive)
    {
      continue;
    }

    if (!s_resolved_desc_build(system, &entry->type, &entry->desc,
            &item->resolved))
    {
      /* An invalid stored description cannot be rebuilt. */
      continue;
    }

    if (!s_patch_instances_generate(&item->resolved, &item->instances,
            &item->terrain_normals, &item->instance_count, NULL,
            &item->candidate_index, NULL))
    {
      for (u32 j = 0u; j <= i; ++j)
      {
        free(rebuilt[j].instances);
        free(rebuilt[j].terrain_normals);
        s_terrain_candidate_index_destroy(rebuilt[j].candidate_index);
      }
      free(rebuilt);
      s_grass.color_noise_image = old_image;
      
      return false;
    }
    s_patch_bounds_calculate(&item->resolved, item->instances,
        item->instance_count, &item->bounds_min, &item->bounds_max);
    item->has_type = true;
  }

  s_batches_release_all();
  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[i];
    LDKGrassPatchRebuild *item = &rebuilt[i];
    u32 batch_index;

    if (!entry->alive)
    {
      continue;
    }

    free(entry->instances);
    free(entry->terrain_normals);
    s_terrain_candidate_index_destroy(entry->candidate_index);
    entry->candidate_index = NULL;
    entry->instances = NULL;
    entry->terrain_normals = NULL;
    entry->instance_count = 0u;
    entry->instance_capacity = 0u;
    entry->instance_offset = 0u;
    entry->batch_index = LDK_GRASS_INVALID_BATCH;

    if (!item->has_type)
    {
      continue;
    }

    batch_index = s_batch_find_or_add(&item->resolved);
    LDK_ASSERT(batch_index != LDK_GRASS_INVALID_BATCH);
    if (batch_index == LDK_GRASS_INVALID_BATCH)
    {
      free(item->instances);
      free(item->terrain_normals);
      s_terrain_candidate_index_destroy(item->candidate_index);
      continue;
    }

    entry->instances = item->instances;
    entry->terrain_normals = item->terrain_normals;
    entry->candidate_index = item->candidate_index;
    item->instances = NULL;
    item->terrain_normals = NULL;
    item->candidate_index = NULL;
    entry->instance_count = item->instance_count;
    entry->instance_capacity = item->instance_count;
    entry->batch_index = batch_index;
    entry->bounds_min = item->bounds_min;
    entry->bounds_max = item->bounds_max;
    s_grass.batches[batch_index].dirty = true;
  }

  free(rebuilt);
  s_grass.config_hash = s_system_config_hash(system);

  return true;
}

void ldk_grass_type_desc_defaults(LDKGrassTypeDesc *out_desc)
{
  if (!out_desc)
  {
    return;
  }

  memset(out_desc, 0, sizeof(*out_desc));
  out_desc->blade_preset = LDK_GRASS_BLADE_PRESET_THIN;
  out_desc->scale_min = vec3_make(1.0f, 1.0f, 1.0f);
  out_desc->scale_max = vec3_make(1.0f, 1.0f, 1.0f);
  out_desc->follow_terrain = true;
  out_desc->density = 1.0f;
  out_desc->clumping = 0.0f;
  out_desc->clump_radius = 0.5f;
  out_desc->min_height = 0.8f;
  out_desc->max_height = 1.0f;
  out_desc->blade_width = 0.10f;
  out_desc->curvature = 0.08f;
  out_desc->wind_strength = 0.08f;
  out_desc->interaction_recovery_time =
      LDK_GRASS_INTERACTION_DEFAULT_RECOVERY_TIME;
  out_desc->bottom_color = 0x35502affu;
  out_desc->top_color = 0x89a95cffu;
  out_desc->noise_color_stop_0_color = 0xcac279ffu;
  out_desc->noise_color_stop_1_color = 0xa6ba6affu;
  out_desc->noise_color_stop_2_color = 0x70ab56ffu;
  out_desc->noise_color_stop_3_color = 0x427d48ffu;

}

bool ldk_grass_interact(
    Vec3 from, Vec3 to, float radius, float strength)
{
  return s_interaction_queue(from, to, radius, strength);
}

void ldk_grass_patch_desc_defaults(LDKGrassPatchDesc *out_desc)
{
  if (!out_desc)
  {
    return;
  }

  memset(out_desc, 0, sizeof(*out_desc));
  out_desc->plane.axis_u = vec3_make(1.0f, 0.0f, 0.0f);
  out_desc->plane.axis_v = vec3_make(0.0f, 0.0f, -1.0f);
  out_desc->density_scale = 1.0f;
  out_desc->terrain_entity = x_handle_null();
  out_desc->density_mask = ldk_asset_image_null();
  out_desc->terrain_vegetation_layer_id = 0u;
}

LDKGrassPatch ldk_grass_patch_null(void)
{
  return (LDKGrassPatch){0u, 0u};
}

bool ldk_grass_patch_is_valid(LDKGrassPatch patch)
{
  return s_patch_get(patch) != NULL;
}

/* All patches receive a complete description: no registry or type lookup. */
LDKGrassPatch ldk_grass_add(const LDKGrassTypeDesc *type,
    const LDKGrassPatchDesc *desc)
{
  LDKGrassResolvedDesc resolved;
  LDKGrassPatchEntry *entry;
  Mat4 *instances;
  Vec3 *terrain_normals;
  LDKGrassTerrainCandidateIndex *candidate_index = NULL;
  u32 instance_count;
  u32 batch_index;
  u32 patch_index;
  Vec3 bounds_min;
  Vec3 bounds_max;

  if (!s_grass.initialized || !s_grass.owner ||
      !s_resolved_desc_build(s_grass.owner, type, desc, &resolved) ||
      !s_patch_instances_generate(&resolved, &instances,
          &terrain_normals, &instance_count, NULL, &candidate_index, NULL))
  {
    s_terrain_candidate_index_destroy(candidate_index);
    return ldk_grass_patch_null();
  }

  batch_index = s_batch_find_or_add(&resolved);
  if (batch_index == LDK_GRASS_INVALID_BATCH)
  {
    free(instances);
    free(terrain_normals);
    s_terrain_candidate_index_destroy(candidate_index);
    return ldk_grass_patch_null();
  }
  s_patch_bounds_calculate(
      &resolved, instances, instance_count, &bounds_min, &bounds_max);

  for (patch_index = 0u; patch_index < s_grass.patch_count; ++patch_index)
  {
    if (!s_grass.patches[patch_index].alive)
    {
      break;
    }
  }

  if (patch_index == s_grass.patch_count)
  {
    if (!s_patches_reserve(s_grass.patch_count + 1u))
    {
      free(instances);
      free(terrain_normals);
      s_terrain_candidate_index_destroy(candidate_index);
      return ldk_grass_patch_null();
    }
    s_grass.patch_count += 1u;
  }

  entry = &s_grass.patches[patch_index];
  if (entry->version == 0u)
  {
    entry->version = 1u;
  }
  entry->desc = *desc;
  entry->type = *type;
  entry->instances = instances;
  entry->terrain_normals = terrain_normals;
  entry->candidate_index = candidate_index;
  entry->instance_count = instance_count;
  entry->instance_capacity = instance_count;
  entry->instance_offset = 0u;
  entry->batch_index = batch_index;
  entry->bounds_min = bounds_min;
  entry->bounds_max = bounds_max;
  entry->terrain_owned = false;
  entry->alive = true;
  s_grass.batches[batch_index].dirty = true;
  return (LDKGrassPatch){patch_index, entry->version};
}

bool ldk_grass_update(LDKGrassPatch patch,
    const LDKGrassTypeDesc *type, const LDKGrassPatchDesc *desc)
{
  LDKGrassPatchEntry *entry = s_patch_get(patch);
  LDKGrassResolvedDesc resolved;
  Mat4 *instances;
  Vec3 *terrain_normals;
  LDKGrassTerrainCandidateIndex *candidate_index = NULL;
  u32 instance_count;
  u32 batch_index;
  u32 old_batch_index;
  Vec3 bounds_min;
  Vec3 bounds_max;

  if (!entry || !s_grass.owner ||
      !s_resolved_desc_build(s_grass.owner, type, desc, &resolved) ||
      !s_patch_instances_generate(&resolved, &instances,
          &terrain_normals, &instance_count, NULL, &candidate_index, NULL))
  {
    s_terrain_candidate_index_destroy(candidate_index);
    return false;
  }

  batch_index = s_batch_find_or_add(&resolved);
  if (batch_index == LDK_GRASS_INVALID_BATCH)
  {
    free(instances);
    free(terrain_normals);
    s_terrain_candidate_index_destroy(candidate_index);
    return false;
  }
  s_patch_bounds_calculate(
      &resolved, instances, instance_count, &bounds_min, &bounds_max);

  old_batch_index = entry->batch_index;
  free(entry->instances);
  free(entry->terrain_normals);
  s_terrain_candidate_index_destroy(entry->candidate_index);
  entry->desc = *desc;
  entry->type = *type;
  entry->instances = instances;
  entry->terrain_normals = terrain_normals;
  entry->candidate_index = candidate_index;
  entry->instance_count = instance_count;
  entry->instance_capacity = instance_count;
  entry->instance_offset = 0u;
  entry->batch_index = batch_index;
  entry->bounds_min = bounds_min;
  entry->bounds_max = bounds_max;
  if (old_batch_index < s_grass.batch_count)
  {
    s_grass.batches[old_batch_index].dirty = true;
  }
  s_grass.batches[batch_index].dirty = true;
  return true;
}

bool ldk_grass_remove(LDKGrassPatch patch)
{
  LDKGrassPatchEntry *entry = s_patch_get(patch);
  u32 batch_index;

  if (!entry)
  {
    return false;
  }

  batch_index = entry->batch_index;
  free(entry->instances);
  free(entry->terrain_normals);
  s_terrain_candidate_index_destroy(entry->candidate_index);
  entry->instances = NULL;
  entry->terrain_normals = NULL;
  entry->candidate_index = NULL;
  entry->instance_count = 0u;
  entry->instance_capacity = 0u;
  entry->instance_offset = 0u;
  entry->batch_index = LDK_GRASS_INVALID_BATCH;
  memset(&entry->type, 0, sizeof(entry->type));
  entry->terrain_owned = false;
  entry->alive = false;
  entry->version += 1u;
  if (entry->version == 0u)
  {
    entry->version = 1u;
  }
  if (batch_index < s_grass.batch_count)
  {
    s_grass.batches[batch_index].dirty = true;
  }
  return true;
}

void ldk_grass_clear(void)
{
  if (!s_grass.initialized)
  {
    return;
  }

  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[i];

    free(entry->instances);
    free(entry->terrain_normals);
    s_terrain_candidate_index_destroy(entry->candidate_index);
    entry->instances = NULL;
    entry->terrain_normals = NULL;
    entry->candidate_index = NULL;
    entry->instance_count = 0u;
    entry->instance_capacity = 0u;
    entry->instance_offset = 0u;
    entry->batch_index = LDK_GRASS_INVALID_BATCH;
    memset(&entry->type, 0, sizeof(entry->type));
    entry->terrain_owned = false;
    entry->alive = false;
    entry->version += 1u;
    if (entry->version == 0u)
    {
      entry->version = 1u;
    }
  }

  for (u32 i = 0u; i < s_grass.batch_count; ++i)
  {
    LDKGrassBatch *batch = &s_grass.batches[i];

    if (ldk_renderer_instance_set_is_valid(
            s_grass.renderer, batch->instance_set))
    {
      ldk_renderer_instance_set_destroy(
          s_grass.renderer, batch->instance_set);
    }
    batch->instance_set = ldk_renderer_instance_set_null();
    if (ldk_renderer_instance_set_is_valid(
            s_grass.renderer, batch->lod_instance_set))
    {
      ldk_renderer_instance_set_destroy(
          s_grass.renderer, batch->lod_instance_set);
    }
    batch->lod_instance_set = ldk_renderer_instance_set_null();
    batch->lod_instance_count = 0u;
    batch->instance_count = 0u;
    batch->lod_dirty = true;
    batch->dirty = false;
  }
  /* A subsequent update should recreate editor-authored terrain patches. */
  s_grass.terrain_sync_hash = UINT64_MAX;
  s_grass.terrain_quick_hash = 0u;
  s_grass.terrain_layer_count = 0u;
}

/* Terrain-authored patches are internal to GrassSystem. Never remove patches
 * supplied by game code; those may share the same grass type and batching. */
static void s_terrain_patches_remove(void)
{
  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[i];
    if (!entry->alive || !entry->terrain_owned)
    {
      continue;
    }
    (void)ldk_grass_remove((LDKGrassPatch){i, entry->version});
  }
  s_grass.terrain_sync_hash = UINT64_MAX;
  s_grass.terrain_quick_hash = 0u;
  s_grass.terrain_layer_count = 0u;
}

/* Only create a patch for terrain tiles whose mask may actually contribute
 * blades. The one-pixel halo conservatively includes bilinear neighbors. */
/* Scene terrain recipes are generated from every enabled TerrainComponent.
 * The hash covers entity identity, transform, component revisions and referenced
 * image revisions. Rebuild only terrain-owned patches when inputs change.
 */
typedef struct LDKGrassTerrainSource
{
  LDKEntity entity;
  LDKTerrain terrain;
  const LDKTerrainComponent *component;
  u64 revision;
  Mat4 world;
} LDKGrassTerrainSource;

typedef struct LDKGrassTerrainCollection
{
  LDKGrassTerrainSource *sources;
  u32 count;
  u32 capacity;
  bool ok;
} LDKGrassTerrainCollection;

static bool s_terrain_source_collect(LDKEntity entity, void *user)
{
  LDKGrassTerrainCollection *list = user;
  const LDKTerrainComponent *component = ldk_ecs_component_get_const(
      entity, LDK_COMPONENT_TYPE_TERRAIN);
  if (!component || !ldk_ecs_component_is_enabled(
          entity, LDK_COMPONENT_TYPE_TERRAIN) ||
      !ldk_terrain_validate(&component->terrain) ||
      !ldk_scenegraph_update_entity(entity)) return true;
  Mat4 world;
  if (!ldk_transform_get_world_matrix(entity, &world)) return true;
  if (list->count == list->capacity)
  {
    u32 capacity = list->capacity ? list->capacity * 2u : 4u;
    LDKGrassTerrainSource *sources = realloc(list->sources,
        (size_t)capacity * sizeof(*sources));
    if (!sources)
    {
      list->ok = false;
      return false;
    }
    list->sources = sources;
    list->capacity = capacity;
  }
  LDKGrassTerrainSource *source = &list->sources[list->count++];
  source->entity = entity;
  source->terrain = component->terrain;
  source->component = component;
  source->revision = component->revision;
  source->world = world;
  return true;
}

/* Layer identity is stable across edits and reorderings. */
static i32 s_terrain_layer_cache_find(LDKEntity entity, u64 layer_id)
{
  for (u32 i = 0u; i < s_grass.terrain_layer_count; ++i)
    if (s_grass.terrain_layers[i].layer_id == layer_id &&
        s_grass.terrain_layers[i].entity.index == entity.index &&
        s_grass.terrain_layers[i].entity.version == entity.version)
      return (i32)i;
  return -1;
}

static bool s_terrain_layer_cache_reserve(u32 required)
{
  if (required <= s_grass.terrain_layer_capacity) return true;
  u32 capacity = s_grass.terrain_layer_capacity
      ? s_grass.terrain_layer_capacity * 2u : 16u;
  if (capacity < required) capacity = required;
  if (!s_allocation_size_is_valid(capacity,
          sizeof(*s_grass.terrain_layers))) return false;
  LDKGrassTerrainLayerCache *items = realloc(s_grass.terrain_layers,
      (size_t)capacity * sizeof(*items));
  if (!items) return false;
  s_grass.terrain_layers = items;
  s_grass.terrain_layer_capacity = capacity;
  return true;
}

/* Conservative reach of clumped positions, plus one texel for bilinear
 * density/normal sampling. Return grid cells, not entire patch tiles: a small
 * brush in the middle of one patch must not invalidate its eight neighbors. */
static u32 s_terrain_region_halo(const LDKTerrain *terrain,
    const LDKTerrainVegetationLayer *layer)
{
  const float reach = layer->type.clumping > 0.0f
      ? 9.0f + layer->type.clump_radius : 0.0f;
  const float cells = 1.0f + ceilf(reach / terrain->cell_size);
  const u64 limit = (u64)terrain->width + terrain->height;
  return cells >= (float)limit ? (u32)(limit > UINT32_MAX
      ? UINT32_MAX : limit) : (u32)cells;
}

/* Intersect an expanded sample rectangle against the actual patch grid
 * rectangle. Expand by sample cells, not by entire neighboring patches. */
static bool s_terrain_tile_in_region(u32 tx, u32 tz,
    const LDKTerrainHeightmapRegion *region, u32 halo)
{
  if (!region) return true;
  const u64 x0 = (u64)tx * LDK_GRASS_TERRAIN_PATCH_CELLS;
  const u64 z0 = (u64)tz * LDK_GRASS_TERRAIN_PATCH_CELLS;
  const u64 x1 = x0 + LDK_GRASS_TERRAIN_PATCH_CELLS;
  const u64 z1 = z0 + LDK_GRASS_TERRAIN_PATCH_CELLS;
  return x0 < (u64)region->x + region->width + halo &&
      z0 < (u64)region->y + region->height + halo &&
      (u64)region->x < x1 + halo &&
      (u64)region->y < z1 + halo;
}

static void s_terrain_layer_patches_remove(LDKEntity entity, u64 layer_id)
{
  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[i];
    if (entry->alive && entry->terrain_owned &&
        entry->desc.terrain_entity.index == entity.index &&
        entry->desc.terrain_entity.version == entity.version &&
        entry->desc.terrain_vegetation_layer_id == layer_id)
      (void)ldk_grass_remove((LDKGrassPatch){i, entry->version});
  }
}

/* Remove only the patches overlapping a sculpted heightmap region.
 * All other patch instance arrays and their mesh/material settings survive. */
static void s_terrain_layer_patches_remove_region(LDKEntity entity,
    u64 layer_id, const LDKTerrainHeightmapRegion *region, u32 halo)
{
  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[i];
    if (entry->alive && entry->terrain_owned &&
        s_entity_equal(entry->desc.terrain_entity, entity) &&
        entry->desc.terrain_vegetation_layer_id == layer_id &&
        s_terrain_tile_in_region(entry->terrain_tile_x,
            entry->terrain_tile_z, region, halo))
      (void)ldk_grass_remove((LDKGrassPatch){i, entry->version});
  }
}

/* Hash authored layer settings only. Density changes have their own revision
 * and dirty region, so a brush can update a subset of patches even when a
 * mask buffer is first allocated or a whole mask is replaced. */
static u64 s_terrain_layer_hash(const LDKGrassTerrainSource *source,
    const LDKTerrainVegetationLayer *layer, u64 asset_revision)
{
  const LDKTerrain *terrain = &source->terrain;
  u64 hash = UINT64_C(14695981039346656037);
#define HASH_FIELD(x) s_hash_bytes(&hash, &(x), sizeof(x))
  HASH_FIELD(source->entity);
  HASH_FIELD(source->world);
  HASH_FIELD(terrain->width);
  HASH_FIELD(terrain->height);
  HASH_FIELD(terrain->cell_size);
  HASH_FIELD(terrain->height_scale);
  HASH_FIELD(terrain->height_offset);
  HASH_FIELD(terrain->height_channel);
  HASH_FIELD(terrain->center_on_origin);
  HASH_FIELD(terrain->origin_x);
  HASH_FIELD(terrain->origin_z);
  HASH_FIELD(layer->id);
  HASH_FIELD(layer->density_scale);
  HASH_FIELD(layer->type);
  /* Terrain vegetation uses embedded R8 masks and procedural blades; no
   * external asset affects its scatter. A global asset-manager revision
   * must not regenerate every vegetation patch while painting surfaces.
   * Mesh-based Tree/Decoration layers still re-evaluate asset revisions. */
  if (layer->type.kind != LDK_GRASS_LAYER_VEGETATION)
    HASH_FIELD(asset_revision);
#undef HASH_FIELD
  return hash;
}

/* Regenerate only one changed layer, leaving every other layer and terrain
 * untouched. A failed rebuild is retried on the following frame. */
static bool s_terrain_layer_patches_create(const LDKGrassSystem *system,
    const LDKGrassTerrainSource *source,
    const LDKTerrainVegetationLayer *layer, const u8 *embedded,
    u32 *patch_total, const LDKTerrainHeightmapRegion *region,
    u32 region_halo, bool only_missing, bool empty_new)
{
  const LDKTerrain *terrain = &source->terrain;
  if (!embedded || layer->density_scale <= 0.0f) return true;
  if (!region)
  {
    bool any = false;
    for (size_t j = 0u; j < (size_t)terrain->width * terrain->height; ++j)
      if (embedded[j]) {any = true; break;}
    if (!any) return true;
  }

  const u32 tiles_x = 1u + (terrain->width - 1u) /
      LDK_GRASS_TERRAIN_PATCH_CELLS;
  const u32 tiles_z = 1u + (terrain->height - 1u) /
      LDK_GRASS_TERRAIN_PATCH_CELLS;
  float origin_x = terrain->origin_x;
  float origin_z = terrain->origin_z;
  if (terrain->center_on_origin)
  {
    origin_x -= (float)terrain->width * terrain->cell_size * 0.5f;
    origin_z -= (float)terrain->height * terrain->cell_size * 0.5f;
  }
  for (u32 tz = 0u; tz < tiles_z; ++tz)
  {
    u32 z0 = tz * LDK_GRASS_TERRAIN_PATCH_CELLS;
    u32 z1 = z0 + LDK_GRASS_TERRAIN_PATCH_CELLS;
    if (z1 < z0 || z1 > terrain->height) z1 = terrain->height;
    for (u32 tx = 0u; tx < tiles_x; ++tx)
    {
      if (!s_terrain_tile_in_region(tx, tz, region, region_halo)) continue;
      u32 x0 = tx * LDK_GRASS_TERRAIN_PATCH_CELLS;
      u32 x1 = x0 + LDK_GRASS_TERRAIN_PATCH_CELLS;
      if (x1 < x0 || x1 > terrain->width) x1 = terrain->width;
      bool covered = false;
      for (u32 z = z0; z < z1 && !covered; ++z)
        for (u32 x = x0; x < x1; ++x)
          if (embedded[(size_t)z * terrain->width + x])
          {
            covered = true;
            break;
          }
      if (!covered) continue;
      if (only_missing)
      {
        bool exists = false;
        for (u32 j = 0u; j < s_grass.patch_count; ++j)
        {
          const LDKGrassPatchEntry *entry = &s_grass.patches[j];
          if (entry->alive && entry->terrain_owned &&
              s_entity_equal(entry->desc.terrain_entity, source->entity) &&
              entry->desc.terrain_vegetation_layer_id == layer->id &&
              entry->terrain_tile_x == tx && entry->terrain_tile_z == tz)
          {
            exists = true;
            break;
          }
        }
        if (exists) continue;
      }
      if (*patch_total >= LDK_GRASS_TERRAIN_PATCH_LIMIT)
      {
        ldk_log_error("Too many terrain vegetation patches.\n");
        return false;
      }
      LDKGrassPatchDesc desc;
      ldk_grass_patch_desc_defaults(&desc);
      desc.plane.origin = vec3_make(
          origin_x + (float)x0 * terrain->cell_size, 0.0f,
          origin_z + (float)z1 * terrain->cell_size);
      desc.plane.axis_u = vec3_make(
          (float)(x1 - x0) * terrain->cell_size, 0.0f, 0.0f);
      desc.plane.axis_v = vec3_make(
          0.0f, 0.0f, -(float)(z1 - z0) * terrain->cell_size);
      desc.density_scale = layer->density_scale;
      desc.seed = s_random_state(system->seed ^
          (u32)layer->id ^ (u32)(layer->id >> 32u) ^
          (tx * 0x9e3779b9u) ^ (tz * 0x85ebca6bu));
      desc.terrain_entity = source->entity;
      desc.terrain_vegetation_layer_id = layer->id;
      /* New tiles found during regional painting start with no instances.
       * Their candidates are generated only inside the touched region by
       * density_refresh(), not over the entire tile. */
      const float final_density_scale = desc.density_scale;
      if (empty_new) desc.density_scale = 0.0f;
      LDKGrassPatch patch = ldk_grass_add(&layer->type, &desc);
      if (!ldk_grass_patch_is_valid(patch)) return false;
      LDKGrassPatchEntry *entry = s_patch_get(patch);
      entry->desc.density_scale = final_density_scale;
      entry->terrain_owned = true;
      entry->terrain_tile_x = tx;
      entry->terrain_tile_z = tz;
      ++*patch_total;
    }
  }
  return true;
}

/* Height-only edits leave candidate X/Z positions and density masks unchanged.
 * Reproject retained instances instead of rerunning expensive random scatter
 * for every brush sample. The saved normals make terrain alignment exact:
 * align(new) * inverse(align(old)) preserves the original random yaw/tilt.
 * Masks, layer settings and full heightmap replacements still rebuild patches.
 */
static bool s_terrain_layer_patches_reproject(
    const LDKGrassSystem *system, const LDKGrassTerrainSource *source,
    const LDKTerrainVegetationLayer *layer,
    const LDKTerrainHeightmapRegion *region, u32 halo)
{
  const u64 perf_start = ldk_os_time_ticks_get();
  u32 modified_patches = 0u;
  u32 modified_instances = 0u;
  u64 inspected_instances = 0u;
  u32 indexed_patches = 0u;
  double scan_ms = 0.0;
  double bounds_ms = 0.0;
  const LDKTerrain *terrain = &source->terrain;
  const Vec3 up = vec3_make(0.0f, 1.0f, 0.0f);
  LDKImageInfo heights = {0};
  const LDKImageInfo *height_source = NULL;
  if (source->component->heightmap)
  {
    if (!ldk_image_get_info(source->component->heightmap, &heights))
      return false;
    height_source = &heights;
  }
  bool invertible = false;
  const Mat4 inverse = mat4_inverse_full(source->world, &invertible);
  if (!invertible) return false;
  float ox = terrain->origin_x;
  float oz = terrain->origin_z;
  if (terrain->center_on_origin)
  {
    ox -= (float)terrain->width * terrain->cell_size * 0.5f;
    oz -= (float)terrain->height * terrain->cell_size * 0.5f;
  }
  /* The bilinear normal also depends on adjacent heightmap cells. */
  const u32 x0 = region->x > 0u ? region->x - 1u : 0u;
  const u32 z0 = region->y > 0u ? region->y - 1u : 0u;
  const u32 x1 = region->x + region->width < terrain->width
      ? region->x + region->width + 1u : terrain->width;
  const u32 z1 = region->y + region->height < terrain->height
      ? region->y + region->height + 1u : terrain->height;
  const float min_x = ox + (float)x0 * terrain->cell_size;
  const float min_z = oz + (float)z0 * terrain->cell_size;
  const float max_x = ox + (float)x1 * terrain->cell_size;
  const float max_z = oz + (float)z1 * terrain->cell_size;
  for (u32 j = 0u; j < s_grass.patch_count; ++j)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[j];
    if (!entry->alive || !entry->terrain_owned ||
        !s_entity_equal(entry->desc.terrain_entity, source->entity) ||
        entry->desc.terrain_vegetation_layer_id != layer->id ||
        !s_terrain_tile_in_region(entry->terrain_tile_x,
            entry->terrain_tile_z, region, halo)) continue;
    if (entry->instance_count && !entry->terrain_normals) return false;
    if (entry->type.kind != LDK_GRASS_LAYER_VEGETATION &&
        !entry->type.follow_terrain) continue;
    bool changed = false;
    const LDKGrassTerrainCandidateIndex *index = entry->candidate_index;
    const bool indexed = index && index->ready &&
        index->candidate_slots && index->slot_candidates;
    if (indexed) ++indexed_patches;
    LDKGrassTerrainCandidateIterator it = {0};
    const LDKGrassTerrainClip clip = {min_x, min_z, max_x, max_z};
    if (indexed)
      s_terrain_candidate_iterator_init(&it, index,
          &entry->desc.plane, &clip);
    u32 cursor = 0u;
    const u64 scan_start = ldk_os_time_ticks_get();
    while (true)
    {
      u32 i;
      Vec3 local;
      if (indexed)
      {
        Vec3 candidate;
        u32 random_state;
        u32 candidate_id;
        if (!s_terrain_candidate_iterator_next(&it,
                &candidate, &random_state, &candidate_id)) break;
        (void)random_state;
        if (!s_terrain_clip_contains(&clip,
                candidate.x, candidate.z)) continue;
        i = index->candidate_slots[candidate_id];
        if (i == UINT32_MAX) continue;
        if (i >= entry->instance_count) return false;
        local = candidate;
      }
      else
      {
        if (cursor >= entry->instance_count) break;
        i = cursor++;
        const Mat4 *instance = &entry->instances[i];
        const Vec3 world_position = vec3_make(instance->m[12],
            instance->m[13], instance->m[14]);
        local = mat4_mul_point(inverse, world_position);
        if (!s_terrain_clip_contains(&clip, local.x, local.z)) continue;
      }
      ++inspected_instances;
      Mat4 *instance = &entry->instances[i];
      float height;
      Vec3 terrain_normal;
      if (!ldk_terrain_surface_sample(terrain, height_source,
              local.x, local.z, &height, &terrain_normal)) continue;
      Vec3 new_normal = vec3_make(
          inverse.m[0] * terrain_normal.x +
              inverse.m[1] * terrain_normal.y +
              inverse.m[2] * terrain_normal.z,
          inverse.m[4] * terrain_normal.x +
              inverse.m[5] * terrain_normal.y +
              inverse.m[6] * terrain_normal.z,
          inverse.m[8] * terrain_normal.x +
              inverse.m[9] * terrain_normal.y +
              inverse.m[10] * terrain_normal.z);
      new_normal = vec3_norm(new_normal);
      const Vec3 new_position = mat4_mul_point(source->world,
          vec3_make(local.x, height, local.z));
      if (entry->type.kind == LDK_GRASS_LAYER_VEGETATION ||
          entry->type.align_to_normal)
      {
        const Quat old_align = quat_from_to(up,
            entry->terrain_normals[i]);
        const Quat new_align = quat_from_to(up, new_normal);
        const Quat delta = quat_mul(new_align,
            quat_unit_inverse(old_align));
        for (u32 axis = 0u; axis < 3u; ++axis)
        {
          Vec3 basis = vec3_make(instance->m[axis * 4u],
              instance->m[axis * 4u + 1u],
              instance->m[axis * 4u + 2u]);
          basis = quat_mul_vec3(delta, basis);
          instance->m[axis * 4u] = basis.x;
          instance->m[axis * 4u + 1u] = basis.y;
          instance->m[axis * 4u + 2u] = basis.z;
        }
      }
      instance->m[12] = new_position.x;
      instance->m[13] = new_position.y;
      instance->m[14] = new_position.z;
      entry->terrain_normals[i] = new_normal;
      ++modified_instances;
      changed = true;
    }
    scan_ms += ldk_os_time_ticks_interval_get_milliseconds(
        scan_start, ldk_os_time_ticks_get());
    if (changed)
    {
      const u64 bounds_start = ldk_os_time_ticks_get();
      ++modified_patches;
      LDKGrassResolvedDesc resolved;
      if (!s_resolved_desc_build(system, &entry->type, &entry->desc,
              &resolved)) return false;
      s_patch_bounds_calculate(&resolved, entry->instances,
          entry->instance_count, &entry->bounds_min, &entry->bounds_max);
      bounds_ms += ldk_os_time_ticks_interval_get_milliseconds(
          bounds_start, ldk_os_time_ticks_get());
      if (entry->batch_index < s_grass.batch_count)
        s_grass.batches[entry->batch_index].dirty = true;
    }
  }
  const double elapsed = ldk_os_time_ticks_interval_get_milliseconds(
      perf_start, ldk_os_time_ticks_get());
  if (elapsed >= 10.0)
  {
    static u32 slow_reproject_count;
    if (++slow_reproject_count <= 24u || slow_reproject_count % 60u == 0u)
      ldk_log_warning("[TerrainPerf] grass reproject: %.2f ms, "
          "patches=%u, indexed=%u, inspected=%llu, updated=%u, "
          "scan=%.2f ms, bounds=%.2f ms\n", elapsed,
          modified_patches, indexed_patches,
          (unsigned long long)inspected_instances,
          modified_instances, scan_ms, bounds_ms);
  }
  return true;
}

/* Matrix storage is dense, while candidate IDs remain stable for the
 * lifetime of the scatter. Removing one accepted candidate swaps the final
 * instance into its slot and updates both lookup directions. */
static void s_terrain_patch_candidate_remove(LDKGrassPatchEntry *entry,
    u32 candidate_id)
{
  LDKGrassTerrainCandidateIndex *index = entry->candidate_index;
  const u32 slot = index->candidate_slots[candidate_id];
  if (slot == UINT32_MAX) return;
  LDK_ASSERT(slot < entry->instance_count);
  const u32 last = --entry->instance_count;
  if (slot != last)
  {
    const u32 moved_id = index->slot_candidates[last];
    entry->instances[slot] = entry->instances[last];
    entry->terrain_normals[slot] = entry->terrain_normals[last];
    index->slot_candidates[slot] = moved_id;
    index->candidate_slots[moved_id] = slot;
  }
  index->candidate_slots[candidate_id] = UINT32_MAX;
}

static bool s_terrain_patch_instance_reserve(LDKGrassPatchEntry *entry,
    u32 needed)
{
  if (needed <= entry->instance_capacity) return true;
  u32 capacity = entry->instance_capacity >= 64u
      ? entry->instance_capacity : 64u;
  while (capacity < needed)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = needed;
      break;
    }
    capacity *= 2u;
  }
  if (entry->candidate_index && capacity > entry->candidate_index->count)
    capacity = entry->candidate_index->count;
  if (capacity < needed ||
      !s_allocation_size_is_valid(capacity, sizeof(*entry->instances)) ||
      !s_allocation_size_is_valid(capacity, sizeof(*entry->terrain_normals)))
    return false;

  Mat4 *instances = realloc(entry->instances,
      (size_t)capacity * sizeof(*entry->instances));
  if (!instances) return false;
  entry->instances = instances;
  Vec3 *normals = realloc(entry->terrain_normals,
      (size_t)capacity * sizeof(*entry->terrain_normals));
  if (!normals) return false;
  entry->terrain_normals = normals;
  entry->instance_capacity = capacity;
  return true;
}

/* Density changes preserve the candidate sequence. Only candidates whose
 * mask acceptance changes need to be inserted into or removed from the
 * existing dense instance array; untouched instances retain their matrices.
 * A full scatter remains necessary when layer configuration changes. */
static bool s_terrain_layer_patches_density_refresh(
    const LDKGrassSystem *system, const LDKGrassTerrainSource *source,
    const LDKTerrainVegetationLayer *layer,
    const LDKTerrainHeightmapRegion *region, u32 halo,
    u32 *out_updated, u32 *out_candidates, u32 *out_indexed)
{
  const LDKTerrain *terrain = &source->terrain;
  float ox = terrain->origin_x;
  float oz = terrain->origin_z;
  if (terrain->center_on_origin)
  {
    ox -= (float)terrain->width * terrain->cell_size * 0.5f;
    oz -= (float)terrain->height * terrain->cell_size * 0.5f;
  }
  /* Bilinear mask filtering reaches half a texel beyond adjacent samples.
   * Include a conservative extra cell to avoid missed/doubled candidates. */
  const float margin = 1.5f * terrain->cell_size;
  const LDKGrassTerrainClip clip = {
    ox + (float)region->x * terrain->cell_size - margin,
    oz + (float)region->y * terrain->cell_size - margin,
    ox + (float)(region->x + region->width) * terrain->cell_size + margin,
    oz + (float)(region->y + region->height) * terrain->cell_size + margin
  };
  bool invertible = false;
  const Mat4 inverse = mat4_inverse_full(source->world, &invertible);
  if (!invertible) return false;
  u32 updated = 0u;
  u32 generated_count = 0u;
  u32 indexed = 0u;
  u64 inspected_instances = 0u;
  double generate_ms = 0.0;
  double compact_ms = 0.0;
  double bounds_ms = 0.0;
  const u64 refresh_start = ldk_os_time_ticks_get();
  for (u32 j = 0u; j < s_grass.patch_count; ++j)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[j];
    if (!entry->alive || !entry->terrain_owned ||
        !s_entity_equal(entry->desc.terrain_entity, source->entity) ||
        entry->desc.terrain_vegetation_layer_id != layer->id ||
        !s_terrain_tile_in_region(entry->terrain_tile_x,
            entry->terrain_tile_z, region, halo)) continue;

    const u64 generate_start = ldk_os_time_ticks_get();
    LDKGrassResolvedDesc resolved;
    if (!s_resolved_desc_build(system, &entry->type, &entry->desc,
            &resolved)) return false;
    Mat4 *new_instances = NULL;
    Vec3 *new_normals = NULL;
    u32 *new_ids = NULL;
    u32 new_count = 0u;
    const bool had_index = entry->candidate_index &&
        entry->candidate_index->ready;
    if (!s_patch_instances_generate(&resolved, &new_instances,
            &new_normals, &new_count, &clip,
            &entry->candidate_index, &new_ids)) return false;
    if (had_index) ++indexed;
    generate_ms += ldk_os_time_ticks_interval_get_milliseconds(
        generate_start, ldk_os_time_ticks_get());
    const u64 compact_start = ldk_os_time_ticks_get();

    LDKGrassTerrainCandidateIndex *candidate_index = entry->candidate_index;
    const bool mapped = candidate_index && candidate_index->ready &&
        candidate_index->candidate_slots &&
        candidate_index->slot_candidates;
    LDKGrassTerrainPlacement mask_placement = {0};
    if (mapped && !s_terrain_placement_resolve(&resolved, &mask_placement))
    {
      free(new_instances);
      free(new_normals);
      free(new_ids);
      return false;
    }
    if (mapped)
    {
      /* First count removals and reserve capacity, so allocation failure
       * cannot leave a partially edited patch. All positions are local to
       * the terrain and stay fixed during Sculpt and density changes. */
      u32 removals = 0u;
      LDKGrassTerrainCandidateIterator it;
      s_terrain_candidate_iterator_init(&it, candidate_index,
          &entry->desc.plane, &clip);
      Vec3 position;
      u32 random_state;
      u32 candidate_id;
      while (s_terrain_candidate_iterator_next(&it, &position,
              &random_state, &candidate_id))
      {
        (void)random_state;
        if (!s_terrain_clip_contains(&clip, position.x, position.z))
          continue;
        ++inspected_instances;
        if (candidate_index->candidate_slots[candidate_id] != UINT32_MAX &&
            s_random_unit(&random_state) >
                s_terrain_mask_sample(&mask_placement,
                    position.x, position.z))
          ++removals;
      }
      /* A mask can change numerically without changing which candidates
       * survive its deterministic threshold. In that case the matrices,
       * bounds and batch GPU data are already correct. */
      if (removals == 0u && new_count == 0u)
      {
        free(new_instances);
        free(new_normals);
        free(new_ids);
        continue;
      }
      if (removals > entry->instance_count ||
          (u64)entry->instance_count - removals + new_count > UINT32_MAX ||
          (entry->instance_count && !entry->terrain_normals))
      {
        free(new_instances);
        free(new_normals);
        free(new_ids);
        return false;
      }
      const u32 target_count = entry->instance_count - removals + new_count;
      if (!s_terrain_patch_instance_reserve(entry, target_count))
      {
        free(new_instances);
        free(new_normals);
        free(new_ids);
        return false;
      }
      s_terrain_candidate_iterator_init(&it, candidate_index,
          &entry->desc.plane, &clip);
      while (s_terrain_candidate_iterator_next(&it, &position,
              &random_state, &candidate_id))
      {
        if (s_terrain_clip_contains(&clip, position.x, position.z) &&
            candidate_index->candidate_slots[candidate_id] != UINT32_MAX &&
            s_random_unit(&random_state) >
                s_terrain_mask_sample(&mask_placement,
                    position.x, position.z))
          s_terrain_patch_candidate_remove(entry, candidate_id);
      }
      for (u32 i = 0u; i < new_count; ++i)
      {
        const u32 id = new_ids[i];
        if (id >= candidate_index->count ||
            candidate_index->candidate_slots[id] != UINT32_MAX)
        {
          free(new_instances);
          free(new_normals);
          free(new_ids);
          return false;
        }
        const u32 slot = entry->instance_count++;
        entry->instances[slot] = new_instances[i];
        entry->terrain_normals[slot] = new_normals[i];
        candidate_index->candidate_slots[id] = slot;
        candidate_index->slot_candidates[slot] = id;
      }
      LDK_ASSERT(entry->instance_count == target_count);
      free(new_instances);
      free(new_normals);
      free(new_ids);
    }
    else
    {
      /* Fallback when the optional candidate index could not be built. */
        inspected_instances += entry->instance_count;
      u32 retained = 0u;
      for (u32 i = 0u; i < entry->instance_count; ++i)
      {
        const Mat4 *matrix = &entry->instances[i];
        const Vec3 local = mat4_mul_point(inverse, vec3_make(
            matrix->m[12], matrix->m[13], matrix->m[14]));
        if (!s_terrain_clip_contains(&clip, local.x, local.z)) ++retained;
      }
      if ((u64)retained + new_count > UINT32_MAX ||
          (retained && !entry->terrain_normals))
      {
        free(new_instances);
        free(new_normals);
        return false;
      }
      const u32 combined_count = retained + new_count;
      /* Reuse patch storage when the accepted count shrinks or stays equal.
       * Avoid a second full-patch allocation/copy for each brush sample. */
      if (combined_count > entry->instance_count)
      {
        if (!s_allocation_size_is_valid(combined_count,
                sizeof(*entry->instances)) ||
            !s_allocation_size_is_valid(combined_count,
                sizeof(*entry->terrain_normals)))
        {
          free(new_instances);
          free(new_normals);
          free(new_ids);
          return false;
        }
        Mat4 *grown = realloc(entry->instances,
            (size_t)combined_count * sizeof(*entry->instances));
        if (!grown)
        {
          free(new_instances);
          free(new_normals);
          free(new_ids);
          return false;
        }
        entry->instances = grown;
        Vec3 *grown_normals = realloc(entry->terrain_normals,
            (size_t)combined_count * sizeof(*entry->terrain_normals));
        if (!grown_normals)
        {
          free(new_instances);
          free(new_normals);
          free(new_ids);
          return false;
        }
        entry->terrain_normals = grown_normals;
        entry->instance_capacity = combined_count;
      }
      u32 out = 0u;
      for (u32 i = 0u; i < entry->instance_count; ++i)
      {
        const Mat4 *matrix = &entry->instances[i];
        const Vec3 local = mat4_mul_point(inverse, vec3_make(
            matrix->m[12], matrix->m[13], matrix->m[14]));
        if (s_terrain_clip_contains(&clip, local.x, local.z)) continue;
        if (out != i)
        {
          entry->instances[out] = *matrix;
          entry->terrain_normals[out] = entry->terrain_normals[i];
        }
        ++out;
      }
      if (new_count)
      {
        memcpy(entry->instances + out, new_instances,
            (size_t)new_count * sizeof(*entry->instances));
        memcpy(entry->terrain_normals + out, new_normals,
            (size_t)new_count * sizeof(*entry->terrain_normals));
      }
      free(new_instances);
      free(new_normals);
      if (!combined_count)
      {
        free(entry->instances);
        free(entry->terrain_normals);
        entry->instances = NULL;
        entry->terrain_normals = NULL;
        entry->instance_capacity = 0u;
      }
      entry->instance_count = combined_count;
      free(new_ids);
    }
    compact_ms += ldk_os_time_ticks_interval_get_milliseconds(
        compact_start, ldk_os_time_ticks_get());
    const u64 bounds_start = ldk_os_time_ticks_get();
    s_patch_bounds_calculate(&resolved, entry->instances,
        entry->instance_count, &entry->bounds_min, &entry->bounds_max);
    bounds_ms += ldk_os_time_ticks_interval_get_milliseconds(
        bounds_start, ldk_os_time_ticks_get());
    if (entry->batch_index < s_grass.batch_count)
      s_grass.batches[entry->batch_index].dirty = true;
    ++updated;
    generated_count += new_count;
  }
  if (out_updated) *out_updated = updated;
  if (out_candidates) *out_candidates = generated_count;
  if (out_indexed) *out_indexed = indexed;
  const double refresh_ms = ldk_os_time_ticks_interval_get_milliseconds(
      refresh_start, ldk_os_time_ticks_get());
  if (refresh_ms >= 25.0)
  {
    static u32 slow_detail_count;
    if (++slow_detail_count <= 24u || slow_detail_count % 60u == 0u)
      ldk_log_warning("[TerrainPerf] grass density detail: %.2f ms, "
          "indexed=%u, inspected=%llu, generated=%u, "
          "generate=%.2f ms, compact=%.2f ms, bounds=%.2f ms\n",
          refresh_ms, indexed, (unsigned long long)inspected_instances,
          generated_count, generate_ms, compact_ms, bounds_ms);
  }
  return true;
}

static bool s_terrain_patches_sync(const LDKGrassSystem *system)
{
  if (!system) return false;
  LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!assets) return false;
  LDKGrassTerrainCollection list = {0};
  list.ok = true;
  (void)ldk_ecs_entity_foreach(s_terrain_source_collect, &list);
  if (!list.ok) {free(list.sources); return false;}

  u64 asset_revision = assets->source ? assets->source->revision : 0u;
  u64 hash = UINT64_C(14695981039346656037);
  s_hash_bytes(&hash, &s_grass.config_hash, sizeof(s_grass.config_hash));
  s_hash_bytes(&hash, &asset_revision, sizeof(asset_revision));
  for (u32 i = 0u; i < list.count; ++i)
  {
    const LDKGrassTerrainSource *source = &list.sources[i];
    s_hash_bytes(&hash, &source->entity, sizeof(source->entity));
    s_hash_bytes(&hash, &source->world, sizeof(source->world));
    s_hash_bytes(&hash, &source->revision, sizeof(source->revision));
  }
  if (hash == s_grass.terrain_sync_hash)
  {
    free(list.sources);
    return true;
  }

  for (u32 i = 0u; i < s_grass.terrain_layer_count; ++i)
    s_grass.terrain_layers[i].seen = false;

  /* Prune deleted/disabled layers before creating new patches, so replacing
   * a layer at the patch limit does not temporarily exceed that limit. */
  for (u32 n = 0u; n < list.count; ++n)
  {
    const LDKGrassTerrainSource *source = &list.sources[n];
    for (u32 i = 0u; i < source->terrain.vegetation_layer_count; ++i)
    {
      i32 index = s_terrain_layer_cache_find(source->entity,
          source->terrain.vegetation_layers[i].id);
      if (index >= 0) s_grass.terrain_layers[index].seen = true;
    }
  }
  for (u32 i = 0u; i < s_grass.terrain_layer_count;)
  {
    if (s_grass.terrain_layers[i].seen) {++i; continue;}
    const LDKGrassTerrainLayerCache stale = s_grass.terrain_layers[i];
    s_terrain_layer_patches_remove(stale.entity, stale.layer_id);
    s_grass.terrain_layers[i] =
        s_grass.terrain_layers[--s_grass.terrain_layer_count];
  }

  u32 patch_total = 0u;
  for (u32 i = 0u; i < s_grass.patch_count; ++i)
    if (s_grass.patches[i].alive && s_grass.patches[i].terrain_owned)
      ++patch_total;

  for (u32 n = 0u; n < list.count; ++n)
  {
    const LDKGrassTerrainSource *source = &list.sources[n];
    const LDKTerrain *terrain = &source->terrain;
    for (u32 i = 0u; i < terrain->vegetation_layer_count; ++i)
    {
      const LDKTerrainVegetationLayer *layer = &terrain->vegetation_layers[i];
      const u8 *embedded = ldk_terrain_component_vegetation_density_get(
          source->component, layer->id);
      const u64 layer_hash = s_terrain_layer_hash(
          source, layer, asset_revision);
      const LDKTerrainVegetationDensity *density =
          source->component->vegetation_density;
      const u64 density_revision = density ? density->revisions[i] : 0u;
      i32 index = s_terrain_layer_cache_find(source->entity, layer->id);
      if (index < 0)
      {
        if (!s_terrain_layer_cache_reserve(s_grass.terrain_layer_count + 1u))
        {
          free(list.sources);
          return false;
        }
        index = (i32)s_grass.terrain_layer_count++;
        s_grass.terrain_layers[index] = (LDKGrassTerrainLayerCache){0};
        s_grass.terrain_layers[index].entity = source->entity;
        s_grass.terrain_layers[index].layer_id = layer->id;
      }
      LDKGrassTerrainLayerCache *cache = &s_grass.terrain_layers[index];
      cache->seen = true;
      const u64 heightmap_revision = source->component->heightmap_revision;
      const bool layer_changed = cache->hash != layer_hash;
      const bool height_changed = cache->heightmap_revision != heightmap_revision;
      const bool density_changed = cache->density_revision != density_revision;
      const bool asset_changed = layer->type.kind !=
          LDK_GRASS_LAYER_VEGETATION && cache->asset_revision != asset_revision;
      if (!layer_changed && !height_changed && !density_changed) continue;

      /* A mask brush modifies only the touched patch tiles; a Sculpt brush
       * merely reprojects existing instances. For simultaneous edits, rebuild
       * the union of both regions. Missing revisions or new layer settings
       * require a full update. */
      const bool height_regional = !height_changed ||
          (source->component->heightmap_grass_dirty_valid &&
           cache->heightmap_revision ==
              source->component->heightmap_grass_dirty_base_revision);
      const bool density_regional = !density_changed ||
          (density && density->dirty_valid[i] &&
           cache->density_revision == density->dirty_base_revisions[i]);
      const bool region_update = !layer_changed && height_regional &&
          density_regional && (height_changed || density_changed);
      LDKTerrainHeightmapRegion merged = {0};
      if (region_update)
      {
        if (height_changed)
          merged = source->component->heightmap_grass_dirty_region;
        if (density_changed)
        {
          const LDKTerrainHeightmapRegion changed = density->dirty_regions[i];
          if (!height_changed) merged = changed;
          else
          {
            const u32 x0 = merged.x < changed.x ? merged.x : changed.x;
            const u32 y0 = merged.y < changed.y ? merged.y : changed.y;
            const u32 x1 = merged.x + merged.width > changed.x + changed.width
                ? merged.x + merged.width : changed.x + changed.width;
            const u32 y1 = merged.y + merged.height > changed.y + changed.height
                ? merged.y + merged.height : changed.y + changed.height;
            merged = (LDKTerrainHeightmapRegion){x0, y0, x1-x0, y1-y0};
          }
        }
      }
      const LDKTerrainHeightmapRegion *region =
          region_update ? &merged : NULL;
      const u32 region_halo = s_terrain_region_halo(terrain, layer);
      if (region_update && !density_changed && height_changed &&
          s_terrain_layer_patches_reproject(
              system, source, layer, region, region_halo))
      {
        cache->hash = layer_hash;
        cache->heightmap_revision = heightmap_revision;
        cache->density_revision = density_revision;
        cache->asset_revision = asset_revision;
        continue;
      }
      /* A density-only brush must not regenerate the expensive complete
       * scatter for each touched tile. Update only candidates whose sampled
       * mask can change, preserving all instances outside the edit area. */
      if (region_update && density_changed && !height_changed)
      {
        const u64 refresh_start = ldk_os_time_ticks_get();
        u32 updated = 0u;
        u32 indexed = 0u;
        /* Newly painted tiles did not have a patch before. Create their
         * metadata cheaply, then refresh the same small region once. */
        patch_total = 0u;
        for (u32 j = 0u; j < s_grass.patch_count; ++j)
          if (s_grass.patches[j].alive && s_grass.patches[j].terrain_owned)
            ++patch_total;
        const u32 old_total = patch_total;
        if (!s_terrain_layer_patches_create(system, source, layer,
                embedded, &patch_total, region, region_halo, true, true))
        {
          cache->hash = 0u;
          free(list.sources);
          return false;
        }
        const u32 created = patch_total - old_total;
        if (!s_terrain_layer_patches_density_refresh(system, source,
                layer, region, region_halo, &updated, NULL, &indexed))
        {
          cache->hash = 0u;
          free(list.sources);
          return false;
        }
        cache->hash = layer_hash;
        cache->heightmap_revision = heightmap_revision;
        cache->density_revision = density_revision;
        cache->asset_revision = asset_revision;
        const double refresh_ms =
            ldk_os_time_ticks_interval_get_milliseconds(
                refresh_start, ldk_os_time_ticks_get());
        if (refresh_ms >= 10.0)
        {
          static u32 slow_refresh_count;
          if (++slow_refresh_count <= 24u || slow_refresh_count % 60u == 0u)
            ldk_log_warning("[TerrainPerf] grass density refresh: %.2f ms, "
                "updated_patches=%u, indexed_patches=%u, "
                "created_patches=%u, patches_total=%u\n",
                refresh_ms, updated, indexed, created, patch_total);
        }
        continue;
      }
      const u64 rebuild_start = ldk_os_time_ticks_get();
      if (region_update)
        s_terrain_layer_patches_remove_region(
            source->entity, layer->id, region, region_halo);
      else
        s_terrain_layer_patches_remove(source->entity, layer->id);
      patch_total = 0u;
      for (u32 j = 0u; j < s_grass.patch_count; ++j)
        if (s_grass.patches[j].alive && s_grass.patches[j].terrain_owned)
          ++patch_total;
      if (!s_terrain_layer_patches_create(system, source, layer,
              embedded, &patch_total, region, region_halo, false, false))
      {
        /* A failure forces a full refresh on retry, ensuring that a
         * partially updated layer cannot be considered synchronized. */
        s_terrain_layer_patches_remove(source->entity, layer->id);
        cache->hash = 0u;
        free(list.sources);
        return false;
      }
      cache->hash = layer_hash;
      cache->heightmap_revision = heightmap_revision;
      cache->density_revision = density_revision;
      cache->asset_revision = asset_revision;
      const double rebuild_ms =
          ldk_os_time_ticks_interval_get_milliseconds(
              rebuild_start, ldk_os_time_ticks_get());
      if (rebuild_ms >= 10.0)
      {
        static u32 slow_rebuild_count;
        if (++slow_rebuild_count <= 24u || slow_rebuild_count % 60u == 0u)
          ldk_log_warning("[TerrainPerf] grass rebuild: %.2f ms, "
              "layer_changed=%u, height_changed=%u, density_changed=%u, "
              "asset_changed=%u, regional=%u, patches_total=%u\n", rebuild_ms,
              layer_changed ? 1u : 0u, height_changed ? 1u : 0u,
              density_changed ? 1u : 0u, asset_changed ? 1u : 0u,
              region_update ? 1u : 0u, patch_total);
      }
    }
  }

  /* Acknowledge only after *all* layers on this terrain have consumed the
   * accumulated edit region. Terrain mesh rendering owns a separate cursor. */
  for (u32 n = 0u; n < list.count; ++n)
  {
    LDKTerrainComponent *component = ldk_ecs_component_get(
        list.sources[n].entity, LDK_COMPONENT_TYPE_TERRAIN);
    if (!component || component->revision != list.sources[n].revision)
      continue;
    if (component->heightmap_grass_dirty_valid)
      component->heightmap_grass_dirty_valid = false;
    if (component->vegetation_density)
      for (u32 i = 0u; i < component->terrain.vegetation_layer_count; ++i)
        component->vegetation_density->dirty_valid[i] = false;
  }
  s_grass.terrain_sync_hash = hash;
  free(list.sources);
  return true;
}

/* Use the submitted game view, which is also used by the existing grass
 * frustum culling. Renderer views have already been submitted before the
 * GrassSystem's render bucket runs. */
static bool s_lod_camera_position(Vec3 *out_position)
{
  const LDKRenderer *renderer = s_grass.renderer;

  if (!out_position || !renderer ||
      renderer->game_view == LDK_RENDERER_VIEW_INVALID)
  {
    return false;
  }

  for (u32 i = 0u; i < renderer->view_count; ++i)
  {
    const LDKRendererView *view = &renderer->views[i];
    Mat4 world;
    Vec3 position;

    if (!view->submitted || view->id != renderer->game_view)
    {
      continue;
    }

    world = mat4_inverse_affine(view->view);
    position = vec3_make(world.m[12], world.m[13], world.m[14]);
    if (!s_vec3_is_finite(position))
    {
      return false;
    }

    *out_position = position;
    return true;
  }

  return false;
}

/* Distance to the nearest point on the patch's AABB, not to its center.
 * Large patches therefore retain detail while any part is near the camera. */
static float s_lod_patch_distance(
    const LDKGrassPatchEntry *entry, Vec3 camera_position)
{
  float dx = fmaxf(fmaxf(entry->bounds_min.x - camera_position.x,
      camera_position.x - entry->bounds_max.x), 0.0f);
  float dy = fmaxf(fmaxf(entry->bounds_min.y - camera_position.y,
      camera_position.y - entry->bounds_max.y), 0.0f);
  float dz = fmaxf(fmaxf(entry->bounds_min.z - camera_position.z,
      camera_position.z - entry->bounds_max.z), 0.0f);
  return sqrtf(dx * dx + dy * dy + dz * dz);
}

static u32 s_lod_instance_count(
    u32 instance_count, float distance, const LDKGrassSystem *system)
{
  float t = s_clamp01((distance - system->lod_start_distance) /
      (system->lod_end_distance - system->lod_start_distance));
  float density;
  double count;

  /* Smoothstep avoids abrupt changes in the density rate at either limit. */
  t = t * t * (3.0f - 2.0f * t);
  density = 1.0f + (system->lod_min_density - 1.0f) * t;
  count = floor((double)instance_count * (double)density + 0.5);
  return count >= (double)instance_count ? instance_count : (u32)count;
}

/* Keep a compact set for all distant blades of one compatible batch. This
 * preserves one draw per distant batch instead of splitting the immutable
 * source instance set into many short per-patch ranges. GPU uploads occur only
 * when the visible LOD selection or source instances actually change. */
static bool s_batch_lod_instances_reserve(
    LDKGrassBatch *batch, u32 required)
{
  u32 capacity;
  Mat4 *instances;
  u32 *colors;

  if (required <= batch->lod_instance_capacity)
  {
    return true;
  }

  capacity = batch->lod_instance_capacity
      ? batch->lod_instance_capacity : 256u;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }
  if (!s_allocation_size_is_valid(capacity, sizeof(Mat4)) ||
      !s_allocation_size_is_valid(capacity, sizeof(u32)))
  {
    return false;
  }

  instances = realloc(batch->lod_instances, (size_t)capacity * sizeof(Mat4));
  if (!instances)
  {
    return false;
  }
  batch->lod_instances = instances;
  colors = realloc(batch->lod_instance_colors,
      (size_t)capacity * sizeof(u32));
  if (!colors)
  {
    return false;
  }
  batch->lod_instance_colors = colors;
  batch->lod_instance_capacity = capacity;
  return true;
}

static bool s_batch_lod_instances_sync(
    u32 batch_index, LDKGrassBatch *batch)
{
  u32 write_index = 0u;

  if (!batch || batch->lod_required_count == 0u)
  {
    return true;
  }

  if (!batch->lod_dirty &&
      batch->lod_selection_hash == batch->lod_frame_hash &&
      batch->lod_instance_count == batch->lod_required_count &&
      ldk_renderer_instance_set_is_valid(
          s_grass.renderer, batch->lod_instance_set))
  {
    return true;
  }

  /* A failed upload can partially change an existing GPU buffer. Force a
   * complete retry even if a later frame selects the previous instance set. */
  batch->lod_dirty = true;
  if (!s_batch_lod_instances_reserve(batch, batch->lod_required_count))
  {
    return false;
  }

  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    const LDKGrassPatchEntry *entry = &s_grass.patches[i];
    u32 count = entry->lod_selected_count;

    if (!entry->alive || entry->batch_index != batch_index || count == 0u)
    {
      continue;
    }

    if (entry->instance_offset > batch->instance_count ||
        count > batch->instance_count - entry->instance_offset ||
        count > batch->lod_required_count - write_index)
    {
      return false;
    }

    memcpy(batch->lod_instances + write_index,
        batch->instances + entry->instance_offset,
        (size_t)count * sizeof(Mat4));
    memcpy(batch->lod_instance_colors + write_index,
        batch->instance_colors + entry->instance_offset,
        (size_t)count * sizeof(u32));
    write_index += count;
  }

  if (write_index != batch->lod_required_count)
  {
    return false;
  }

  if (ldk_renderer_instance_set_is_valid(
          s_grass.renderer, batch->lod_instance_set))
  {
    if (!ldk_renderer_instance_set_update_colored(s_grass.renderer,
            batch->lod_instance_set, batch->lod_instances,
            batch->lod_instance_colors, write_index))
    {
      return false;
    }
  }
  else
  {
    batch->lod_instance_set = ldk_renderer_instance_set_create_colored(
        s_grass.renderer, batch->lod_instances,
        batch->lod_instance_colors, write_index);
    if (!ldk_renderer_instance_set_is_valid(
            s_grass.renderer, batch->lod_instance_set))
    {
      return false;
    }
  }

  batch->lod_instance_count = write_index;
  batch->lod_selection_hash = batch->lod_frame_hash;
  batch->lod_dirty = false;
  return true;
}

static void s_batch_submit_range(LDKGrassBatch *batch,
    LDKResourceMesh mesh, LDKResourceInstanceSet instances,
    u32 offset, u32 count, u32 flags)
{
  if (batch->kind == LDK_GRASS_LAYER_VEGETATION)
  {
    (void)ldk_renderer_submit_mesh_instance_set_range(s_grass.renderer,
        LDK_RENDERER_VIEW_ALL, mesh, batch->material, 0u,
        (mesh.id == batch->lod_mesh.id && mesh.id != batch->mesh.id)
            ? batch->lod_index_count : batch->index_count,
        instances, offset, count, flags);
    return;
  }
  for (u32 i = 0u; i < batch->submesh_count; ++i)
  {
    const LDKMeshSubmesh *sub = &batch->submeshes[i];
    (void)ldk_renderer_submit_mesh_instance_set_range(s_grass.renderer,
        LDK_RENDERER_VIEW_ALL, mesh,
        batch->slot_materials[sub->material_slot], sub->first_index,
        sub->index_count, instances, offset, count, flags);
  }
}

static void s_batch_submit_pending(LDKGrassBatch *batch)
{
  u32 flags;

  if (!batch || batch->submit_count == 0u)
  {
    return;
  }

  flags = batch->casts_shadows
      ? LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS
      : LDK_RENDERER_MESH_SUBMIT_FLAG_NONE;
  s_batch_submit_range(batch, batch->mesh, batch->instance_set,
      batch->submit_offset, batch->submit_count, flags);
  batch->submit_count = 0u;
}

static void s_grass_submit(void)
{
  const LDKGrassSystem *system = s_grass.owner;
  Vec3 camera_position = vec3_make(0.0f, 0.0f, 0.0f);
  bool lod_enabled;

  if (!s_grass.initialized || !s_grass.renderer || !system)
  {
    return;
  }

  lod_enabled = isfinite(system->lod_start_distance) &&
      isfinite(system->lod_end_distance) &&
      isfinite(system->lod_min_density) &&
      system->lod_start_distance >= 0.0f &&
      system->lod_end_distance > system->lod_start_distance &&
      system->lod_min_density >= 0.0f &&
      system->lod_min_density <= 1.0f &&
      s_lod_camera_position(&camera_position);

  for (u32 i = 0u; i < s_grass.batch_count; ++i)
  {
    LDKGrassBatch *batch = &s_grass.batches[i];

    batch->submit_count = 0u;
    batch->submit_ready = false;
    batch->lod_required_count = 0u;
    batch->lod_frame_hash = UINT64_C(14695981039346656037);
    if (batch->dirty && !s_batch_rebuild(i))
    {
      continue;
    }
    if (batch->instance_count == 0u)
    {
      continue;
    }
    if (!ldk_renderer_mesh_is_valid(s_grass.renderer, batch->mesh) &&
        !s_batch_mesh_create(batch, LDK_GRASS_BLADE_LOD_FULL))
    {
      continue;
    }
    if (!ldk_renderer_material_is_valid(
            s_grass.renderer, batch->material) &&
        !s_batch_material_create(batch))
    {
      continue;
    }
    batch->submit_ready = ldk_renderer_instance_set_is_valid(
        s_grass.renderer, batch->instance_set);
  }

  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    LDKGrassPatchEntry *entry = &s_grass.patches[i];
    LDKGrassBatch *batch;
    bool use_lod = false;
    u32 visible_count;

    entry->lod_selected_count = 0u;
    if (!entry->alive || entry->instance_count == 0u ||
        entry->batch_index >= s_grass.batch_count)
    {
      continue;
    }

    batch = &s_grass.batches[entry->batch_index];
    if (!batch->submit_ready)
    {
      continue;
    }

    if (!ldk_renderer_view_bounds_visible(s_grass.renderer,
            LDK_RENDERER_VIEW_ALL,
            entry->bounds_min, entry->bounds_max))
    {
      s_batch_submit_pending(batch);
      continue;
    }

    visible_count = entry->instance_count;
    if (lod_enabled)
    {
      float distance = s_lod_patch_distance(entry, camera_position);
      if (isfinite(distance) && distance > system->lod_start_distance &&
          (ldk_renderer_mesh_is_valid(s_grass.renderer, batch->lod_mesh) ||
              s_batch_mesh_create(
                  batch, LDK_GRASS_BLADE_LOD_SIMPLIFIED)))
      {
        use_lod = true;
        visible_count = s_lod_instance_count(
            entry->instance_count, distance, system);
      }
    }

    if (use_lod)
    {
      s_batch_submit_pending(batch);
      if (visible_count > 0u &&
          batch->lod_required_count <= UINT32_MAX - visible_count)
      {
        entry->lod_selected_count = visible_count;
        batch->lod_required_count += visible_count;
        s_hash_bytes(&batch->lod_frame_hash, &i, sizeof(i));
        s_hash_bytes(&batch->lod_frame_hash,
            &entry->instance_offset, sizeof(entry->instance_offset));
        s_hash_bytes(&batch->lod_frame_hash,
            &visible_count, sizeof(visible_count));
      }
      else if (visible_count > 0u)
      {
        /* A size overflow falls back to unthinned near instances. */
        use_lod = false;
      }
    }

    if (!use_lod)
    {
      if (batch->submit_count != 0u &&
          entry->instance_offset !=
              batch->submit_offset + batch->submit_count)
      {
        s_batch_submit_pending(batch);
      }
      if (batch->submit_count == 0u)
      {
        batch->submit_offset = entry->instance_offset;
      }
      batch->submit_count += entry->instance_count;
    }
  }

  for (u32 i = 0u; i < s_grass.batch_count; ++i)
  {
    LDKGrassBatch *batch = &s_grass.batches[i];
    u32 flags;

    s_batch_submit_pending(batch);
    if (!batch->submit_ready || batch->lod_required_count == 0u)
    {
      continue;
    }

    if (!s_batch_lod_instances_sync(i, batch))
    {
      /* Do not drop grass if the compact LOD instance set cannot be built. */
      for (u32 j = 0u; j < s_grass.patch_count; ++j)
      {
        const LDKGrassPatchEntry *entry = &s_grass.patches[j];
        if (entry->alive && entry->batch_index == i &&
            entry->lod_selected_count != 0u)
        {
          s_batch_submit_range(batch, batch->mesh, batch->instance_set,
              entry->instance_offset, entry->instance_count,
              batch->casts_shadows
                  ? LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS
                  : LDK_RENDERER_MESH_SUBMIT_FLAG_NONE);
        }
      }
      continue;
    }

    flags = batch->casts_shadows
        ? LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS
        : LDK_RENDERER_MESH_SUBMIT_FLAG_NONE;
    s_batch_submit_range(batch, batch->lod_mesh, batch->lod_instance_set,
        0u, batch->lod_required_count, flags);
  }
}

int ldk_grass_system_initialize(void *data)
{
  LDKGrassSystem *system = (LDKGrassSystem *)data;
  LDKRenderer *renderer = (LDKRenderer *)ldk_module_get(LDK_MODULE_RENDERER);

  if (s_grass_editor_preview_active && s_grass.initialized)
  {
    ldk_grass_system_terminate(s_grass.owner);
    s_grass_editor_preview_active = false;
  }
  s_color_noise_defaults(system);
  if (!system || !renderer || s_grass.initialized ||
      !s_system_config_is_valid(system))
  {
    return -1;
  }

  memset(&s_grass, 0, sizeof(s_grass));
  s_grass.owner = system;
  s_grass.renderer = renderer;
  s_grass.interaction_texture = ldk_renderer_texture_null();
  s_grass.color_noise_image = s_color_noise_resolve(system);
  if (!x_handle_is_null(system->color_noise_texture.h) &&
      !s_grass.color_noise_image)
  {
    memset(&s_grass, 0, sizeof(s_grass));
    return -1;
  }

  if (!s_interaction_field_allocate() || !s_interaction_texture_create())
  {
    s_interaction_field_release();

    memset(&s_grass, 0, sizeof(s_grass));
    return -1;
  }

  s_grass.config_hash = s_system_config_hash(system);
  s_grass.terrain_sync_hash = UINT64_MAX;
  s_grass.terrain_quick_hash = 0u;
  s_grass.initialized = true;
  return 0;
}

void ldk_grass_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  LDKGrassSystem *system = (LDKGrassSystem *)data;
  u64 config_hash;

  (void)group;

  if (!system || !s_grass.initialized || s_grass.owner != system ||
      !isfinite(dt) || dt < 0.0f)
  {
    return;
  }

  const u64 perf_start = ldk_os_time_ticks_get();
  config_hash = s_system_config_hash(system);
  if (config_hash != s_grass.config_hash)
  {
    /* Remove terrain-authored recipes before style regeneration. This also
     * avoids referencing stale masks when terrain data changes. */
    s_terrain_patches_remove();
    if (!s_config_rebuild(system))
    {
      return;
    }
  }
  if (!s_terrain_patches_sync(system))
  {
    return;
  }
  const u64 perf_sync_end = ldk_os_time_ticks_get();

  s_interaction_components_collect();
  if (!s_interaction_field_update(dt))
  {
    s_grass.interaction_count = 0u;
  }

  s_grass_submit();
  const u64 perf_end = ldk_os_time_ticks_get();
  const double elapsed = ldk_os_time_ticks_interval_get_milliseconds(
      perf_start, perf_end);
  if (elapsed >= 5.0)
  {
    static u32 slow_grass_count;
    ++slow_grass_count;
    if (slow_grass_count <= 24u || slow_grass_count % 60u == 0u)
      ldk_log_warning("[TerrainPerf] grass: %.2f ms, "
          "sync=%.2f ms, interaction+submit=%.2f ms, "
          "patches=%u\n", elapsed,
          ldk_os_time_ticks_interval_get_milliseconds(perf_start,
              perf_sync_end),
          ldk_os_time_ticks_interval_get_milliseconds(perf_sync_end,
              perf_end), s_grass.patch_count);
  }
}

void ldk_grass_system_on_editor_update(
    u64 system_id, void *data, float dt)
{
  (void)system_id;
  LDKGrassSystem *system = (LDKGrassSystem *)data;
  if (!system || (s_grass.initialized && !s_grass_editor_preview_active))
    return;
  if (s_grass_editor_preview_active && s_grass.owner != system)
  {
    ldk_grass_system_terminate(s_grass.owner);
    s_grass_editor_preview_active = false;
  }
  if (!s_grass.initialized)
  {
    if (ldk_grass_system_initialize(system) != 0) return;
    s_grass_editor_preview_active = true;
  }
  ldk_grass_system_update(system, NULL, dt);
}

LDK_API void ldk_grass_system_editor_clear(void)
{
  if (s_grass_editor_preview_active && s_grass.initialized)
    ldk_grass_system_terminate(s_grass.owner);
  s_grass_editor_preview_active = false;
}

/* A decoration query reads the retained terrain patches, not the submitted
 * instance set. Hence frustum culling cannot remove gameplay results. */
u32 ldk_decoration_query_internal(LDKEntity terrain_entity,
    float x, float z, float radius, LDKDecorationQueryFn fn,
    void *user, ...)
{
  if (!s_grass.initialized || !fn || !isfinite(x) || !isfinite(z) ||
      !isfinite(radius) || radius < 0.0f) return 0u;
  u64 selected[LDK_TERRAIN_VEGETATION_LAYER_COUNT] = {0};
  u32 selected_count = 0u;
  bool all = false;
  va_list args;
  va_start(args, user);
  for (;;)
  {
    const u64 id = va_arg(args, u64);
    if (id == 0u) break;
    if (id == LDK_DECORATION_ALL) { all = true; break; }
    if (selected_count < LDK_TERRAIN_VEGETATION_LAYER_COUNT)
      selected[selected_count++] = id;
  }
  va_end(args);
  if (!all && !selected_count) return 0u;
  const float radius_sq = radius * radius;
  u32 visited = 0u;
  for (u32 i = 0u; i < s_grass.patch_count; ++i)
  {
    const LDKGrassPatchEntry *patch = &s_grass.patches[i];
    if (!patch->alive || !patch->terrain_owned ||
        patch->type.kind == LDK_GRASS_LAYER_VEGETATION || !patch->type.mesh_path.length ||
        !s_entity_equal(patch->desc.terrain_entity, terrain_entity))
      continue;
    if (!all)
    {
      bool matches = false;
      for (u32 j = 0u; j < selected_count; ++j)
        if (selected[j] == patch->desc.terrain_vegetation_layer_id)
        { matches = true; break; }
      if (!matches) continue;
    }
    /* Conservative XZ patch AABB test. Extents include clumped blades. */
    float closest_x = fmaxf(patch->bounds_min.x,
        fminf(x, patch->bounds_max.x));
    float closest_z = fmaxf(patch->bounds_min.z,
        fminf(z, patch->bounds_max.z));
    if ((closest_x-x)*(closest_x-x)+(closest_z-z)*(closest_z-z) >
        radius_sq) continue;
    for (u32 n = 0u; n < patch->instance_count; ++n)
    {
      Mat4 transform = patch->instances[n];
      Vec3 position = vec3_make(transform.m[12], transform.m[13],
          transform.m[14]);
      float dx = position.x - x, dz = position.z - z;
      if (dx*dx + dz*dz > radius_sq) continue;
      LDKDecorationHit hit = {0};
      hit.terrain_entity = terrain_entity;
      hit.layer_id = patch->desc.terrain_vegetation_layer_id;
      hit.candidate_index = n; /* Patch-local ordinal, not a persistent ID. */
      hit.world = transform;
      hit.position = position;
      ++visited;
      if (!fn(&hit, user)) return visited;
    }
  }
  return visited;
}

void ldk_grass_system_terminate(void *data)
{
  LDKGrassSystem *system = (LDKGrassSystem *)data;

  if (!s_grass.initialized || s_grass.owner != system)
  {
    return;
  }

  ldk_grass_clear();
  s_batches_release_all();
  s_interaction_field_release();
  free(s_grass.patches);
  free(s_grass.batches);
  free(s_grass.terrain_layers);
  memset(&s_grass, 0, sizeof(s_grass));
}
