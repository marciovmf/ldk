/**
 * # Grass system
 *
 * Procedural, instanced grass patches with per-type appearance, distribution,
 * wind, interaction and optional color/height noise.
 *
 * ## Grass types and patches
 *
 * A GrassSystem has up to `LDK_GRASS_TYPE_COUNT` (16) named grass types. Each
 * type selects an LDKGrassBladePreset and controls density, blade dimensions,
 * colors and animation. Types can be configured in the GrassSystem inspector
 * or added at runtime with `ldk_grass_type_register()` while the system is
 * active.
 * `ldk_grass_add()` populates a rectangular plane using one configured type;
 * patches can later be changed with `ldk_grass_update()` or removed with
 * `ldk_grass_remove()`. The engine owns the individual blade instances.
 *
 * ## Custom blade meshes
 *
 * In addition to the built-in presets, a game can register generators for
 * `LDK_GRASS_BLADE_PRESET_CUSTOM_0` through `LDK_GRASS_BLADE_PRESET_CUSTOM_7`.
 * These are eight fixed slots, not eight additional grass types. Multiple
 * grass types may select the same custom preset with different settings.
 *
 * A generator is called when a shared batch mesh needs to be created or
 * rebuilt, not once for each blade. It receives the resolved blade profile
 * and bottom/top colors and fills an LDKMeshData. The engine uploads the mesh,
 * instances it, applies wind/interaction and renders its shadows.
 *
 * Generator requirements:
 * - Allocate vertices and indices with malloc/calloc-compatible storage.
 *   The engine calls `ldk_mesh_data_destroy()` after the upload (or on failure).
 * - Use Y in [0, 1], rooted at Y = 0. Scale to the configured blade height
 *   is applied by the system's instance transforms.
 * - Keep X and Z within +/- half the largest shape->profile[].width. This
 *   restriction is required by the current patch bounds and culling.
 * - Set UV.y in [0, 1]: 0 is the fixed root, 1 receives the full deformation.
 *   Additional height segments produce a smoother bend.
 * - Supply normals and valid triangle indices. If has_tangents is true,
 *   initialize each vertex tangent; tangent.w = 0 disables its bend-normal
 *   contribution. Vertex colors use `LDK_RGBA32()` for rgba32 inputs.
 * - Do not allocate RHI/Renderer resources inside the generator.
 *
 * Example: a minimal two-sided ribbon. In the game's C source, include
 * stdlib.h for `calloc()`. * The example uses only four vertices for clarity.
 *
 * ```
 * static bool game_grass_ribbon(const LDKGrassBladeShape *shape,
 *     rgba32 bottom_color, rgba32 top_color, LDKMeshData *out_mesh)
 * {
 *   static const u32 triangles[12] = {
 *       0u, 1u, 3u, 0u, 3u, 2u, 3u, 1u, 0u, 2u, 3u, 0u};
 *   float half_width;
 *
 *   if (!shape || !out_mesh)
 *   {
 *     return false;
 *   }
 *
 *   *out_mesh = (LDKMeshData){0};
 *   half_width = shape->profile[0].width * 0.5f;
 *   out_mesh->vertices = calloc(4u, sizeof(LDKMeshVertex));
 *   out_mesh->indices = calloc(12u, sizeof(u32));
 *   if (!out_mesh->vertices || !out_mesh->indices)
 *   {
 *     ldk_mesh_data_destroy(out_mesh);
 *     return false;
 *   }
 *
 *   for (u32 i = 0u; i < 4u; ++i)
 *   {
 *     float height = i >= 2u ? 1.0f : 0.0f;
 *     float side = (i & 1u) ? 1.0f : 0.0f;
 *     LDKMeshVertex *vertex = &out_mesh->vertices[i];
 *     vertex->position = vec3_make(
 *         side ? half_width : -half_width, height, 0.0f);
 *     vertex->normal = vec3_make(0.0f, 0.0f, 1.0f);
 *     vertex->uv = vec2_make(side, height);
 *     vertex->color = LDK_RGBA32(height > 0.0f
 *         ? top_color : bottom_color);
 *     vertex->tangent = vec4_make(0.0f, 0.0f, 1.0f, 1.0f);
 *   }
 *   for (u32 i = 0u; i < 12u; ++i)
 *   {
 *     out_mesh->indices[i] = triangles[i];
 *   }
 *
 *   out_mesh->vertex_count = 4u;
 *   out_mesh->index_count = 12u;
 *   out_mesh->has_tangents = true;
 *   return true;
 * }
 * ```
 *
 * Register the callback before creating grass that uses `CUSTOM_0`. Then
 * select `CUSTOM_0` in an existing GrassSystem type or register a type at
 * runtime. This example runs after the GrassSystem has initialized:
 *
 * ```
 * ldk_grass_blade_generator_register(
 *     LDK_GRASS_BLADE_PRESET_CUSTOM_0, game_grass_ribbon);
 *
 * LDKGrassTypeDesc type;
 * ldk_grass_type_desc_defaults(&type);
 * x_smallstr_init(&type.name, "Reeds");
 * type.blade_preset = LDK_GRASS_BLADE_PRESET_CUSTOM_0;
 * type.blade_width = 0.2f;
 * LDKGrassTypeId type_id = ldk_grass_type_register(&type);
 * if (type_id != LDK_GRASS_TYPE_ID_INVALID)
 * {
 *   LDKGrassPatchDesc patch;
 *   ldk_grass_patch_desc_defaults(&patch);
 *   patch.plane.axis_u = vec3_make(5.0f, 0.0f, 0.0f);
 *   patch.plane.axis_v = vec3_make(0.0f, 0.0f, -5.0f);
 *   ldk_grass_add(type_id, &patch);
 * }
 * ```
 *
 * A missing generator makes the corresponding custom batch fail rather than
 * silently choosing another preset. Registering a different callback for an
 * occupied slot triggers regeneration on the next GrassSystem update.
 * Registrations survive scene/system reinitialization. Before unloading a
 * game DLL containing a callback, unregister it with:
 *
 * ```
 * ldk_grass_blade_generator_register(
 *     LDK_GRASS_BLADE_PRESET_CUSTOM_0, NULL);
 * ```
 *
 * ## Interaction
 *
 * ldk_grass_interact(from, to, radius, strength) queues a world-space capsule
 * of influence. Equal endpoints produce a circular interaction. The active
 * GrassSystem consumes queued interactions during its render update.
 */

#ifndef LDK_GRASS_H
#define LDK_GRASS_H

#include <ldk_common.h>
#include <ldk_mesh.h>
#include <ldk_asset.h>
#include <module/ldk_system.h>
#include <stdx/stdx_math.h>
#include <stdx/stdx_string.h>
#include <stdx/stdx_array.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @group ldk.system.grass_system */
#define LDK_GRASS_PROFILE_POINT_COUNT 4u
/** @group ldk.system.grass_system */
#define LDK_GRASS_MIN_SIDE_COUNT 3u
/** @group ldk.system.grass_system */
#define LDK_GRASS_MAX_SIDE_COUNT 8u
/** @group ldk.system.grass_system */
#define LDK_GRASS_TYPE_COUNT 16u
/** @group ldk.system.grass_system */
#define LDK_GRASS_CUSTOM_BLADE_PRESET_COUNT 8u
/** @group ldk.system.grass_system */
#define LDK_GRASS_TYPE_ID_INVALID UINT32_MAX

/** @group ldk.system.grass_system */
typedef u32 LDKGrassTypeId;

/** @group ldk.system.grass_system */
typedef enum LDKGrassBladeTopology
{
  LDK_GRASS_BLADE_TOPOLOGY_RIBBON = 0,
  LDK_GRASS_BLADE_TOPOLOGY_CROSSED_RIBBONS,
  LDK_GRASS_BLADE_TOPOLOGY_RADIAL,
  LDK_GRASS_BLADE_TOPOLOGY_TRIPLE_RIBBONS,
  LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM,
  LDK_GRASS_BLADE_TOPOLOGY_PLUMED_STEM,
  LDK_GRASS_BLADE_TOPOLOGY_CUSTOM,
  /** Deprecated source-compatible name for the original thorny prototype. */
  LDK_GRASS_BLADE_TOPOLOGY_THORNY_RIBBON =
      LDK_GRASS_BLADE_TOPOLOGY_THORNY_STEM
} LDKGrassBladeTopology;

/** @group ldk.system.grass_system */
//@enum
typedef enum LDKGrassBladePreset
{
  LDK_GRASS_BLADE_PRESET_THIN = 0,
  LDK_GRASS_BLADE_PRESET_CROSSED,
  LDK_GRASS_BLADE_PRESET_TUFT,
  LDK_GRASS_BLADE_PRESET_FLAT_TOP,
  LDK_GRASS_BLADE_PRESET_THORNY,
  LDK_GRASS_BLADE_PRESET_PLUMED_STEM,
  LDK_GRASS_BLADE_PRESET_CUSTOM_0,
  LDK_GRASS_BLADE_PRESET_CUSTOM_1,
  LDK_GRASS_BLADE_PRESET_CUSTOM_2,
  LDK_GRASS_BLADE_PRESET_CUSTOM_3,
  LDK_GRASS_BLADE_PRESET_CUSTOM_4,
  LDK_GRASS_BLADE_PRESET_CUSTOM_5,
  LDK_GRASS_BLADE_PRESET_CUSTOM_6,
  LDK_GRASS_BLADE_PRESET_CUSTOM_7
} LDKGrassBladePreset;

/**
 * One sample of the normalized blade profile. Height is in [0, 1]. Width is
 * the full ribbon width, or the radial diameter, at that height.
 * @group ldk.system.grass_system
 */
typedef struct LDKGrassProfilePoint
{
  float height;
  float width;
} LDKGrassProfilePoint;

/** Shape shared by every blade in one visually compatible render batch.
 * @group ldk.system.grass_system
 */
typedef struct LDKGrassBladeShape
{
  LDKGrassProfilePoint profile[LDK_GRASS_PROFILE_POINT_COUNT];
  LDKGrassBladeTopology topology;
  /** Used by the radial and thorny stem topologies. */
  u32 side_count;
  /** Used only by the CUSTOM topology; 0..7. */
  u32 custom_slot;
} LDKGrassBladeShape;

/**
 * Generate one reusable blade mesh for a registered CUSTOM preset.
 * The returned LDKMeshData must own malloc-allocated vertices and indices;
 * the engine releases both via ldk_mesh_data_destroy after GPU upload.
 * Mesh coordinates use normalized height Y in [0,1], and X/Z must remain
 * within half the maximum shape profile width, relative to the origin.
 * UV.y is the normalized deformation coordinate, from 0 at the root to 1
 * at the tip. A tangent with w != 0 enables bent-normal computation.
 * This callback must not create renderer resources.
 * @group ldk.system.grass_system
 */
typedef bool (*LDKGrassBladeMeshGenerator)(
    const LDKGrassBladeShape *shape, rgba32 bottom_color,
    rgba32 top_color, LDKMeshData *out_mesh);

/**
 * Flat target geometry. The complete parallelogram, including its edges, is a
 * valid generation area. origin + axis_u + axis_v is the opposite corner.
 * @group ldk.system.grass_system
 */
typedef struct LDKGrassPlane
{
  Vec3 origin;
  Vec3 axis_u;
  Vec3 axis_v;
} LDKGrassPlane;

/** Runtime description used when registering an additional grass type.
 * @group ldk.system.grass_system
 */
typedef struct LDKGrassTypeDesc
{
  XSmallstr name;
  LDKGrassBladePreset blade_preset;
  //@inspect min=0
  float density;
  /** Tendency to grow near other blades: 0 = uniform, 1 = clustered. */
  //@inspect slider min=0 max=1
  float clumping;
  /** Maximum distance from a clump root, in world units. */
  //@inspect slider min=0 max=3
  float clump_radius;
  //@inspect min=0
  float min_height;
  //@inspect min=0
  float max_height;
  /** Use the noise texture to interpolate between min_height and max_height. */
  bool use_noise_height;
  //@inspect min=0
  float blade_width;
  //@inspect slider min=0 max=0.5
  float curvature;
  /** Per-type maximum wind displacement before the system wind force. */
  //@inspect slider min=0 max=0.5
  float wind_strength;
  /** Seconds until interaction deformation fully returns to rest. Zero uses the default. */
  //@inspect slider min=0 max=16
  float interaction_recovery_time;
  //@inspect widget=COLOR
  u32 bottom_color;
  //@inspect widget=COLOR
  u32 top_color;
  bool use_noise_color;
  bool use_custom_noise_palette;
  //@inspect widget=COLOR
  u32 noise_color_stop_0_color;
  //@inspect widget=COLOR
  u32 noise_color_stop_1_color;
  //@inspect widget=COLOR
  u32 noise_color_stop_2_color;
  //@inspect widget=COLOR
  u32 noise_color_stop_3_color;
} LDKGrassTypeDesc;

X_ARRAY_TYPE(LDKGrassTypeDesc)


/** Per-region state. Visual configuration comes from LDKGrassSystem.
 * @group ldk.system.grass_system
 */
typedef struct LDKGrassPatchDesc
{
  LDKGrassPlane plane;
  float density_scale;
  u32 seed;
} LDKGrassPatchDesc;

/** Opaque identity of one patch. Individual blades are intentionally hidden.
 * @group ldk.system.grass_system
 */
typedef struct LDKGrassPatch
{
  u32 index;
  u32 version;
} LDKGrassPatch;

/**
 * @brief Initialize the grass system.
 * @param data Source data or system-owned context for this operation.
 * @return System initialization status code.
 * @group ldk.system.grass_system
 */
int ldk_grass_system_initialize(void *data);
/**
 * @brief Update the grass system.
 * @param data Source data or system-owned context for this operation.
 * @param group Entity group used for this system update.
 * @param dt Elapsed time since the preceding update, in seconds.
 * @group ldk.system.grass_system
 */
void ldk_grass_system_update(
    void *data, const LDKEntityGroup *group, float dt);
/**
 * @brief Shut down the grass system.
 * @param data Source data or system-owned context for this operation.
 * @group ldk.system.grass_system
 */
void ldk_grass_system_terminate(void *data);

/**
 * Register or unregister the mesh generator for CUSTOM_0 through CUSTOM_7.
 * Pass NULL to unregister before unloading game code containing the callback.
 * Registrations survive GrassSystem scene initialization and termination.
 * Re-registering an occupied slot rebuilds affected grass batches on update.
 * @return false for non-CUSTOM presets; true otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_blade_generator_register(
    LDKGrassBladePreset preset, LDKGrassBladeMeshGenerator generator);

/** @group ldk.system.grass_system */
//@system name=GrassSystem initialize=ldk_grass_system_initialize update=ldk_grass_system_update terminate=ldk_grass_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_ENGINE_NATIVE|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED bucket=LDK_SYSTEM_BUCKET_RENDER
typedef struct LDKGrassSystem
{
  //@begin_group "Blade"
  //@inspect slider min=0 max=89
  float min_tilt_degrees;
  //@inspect slider min=0 max=89
  float max_tilt_degrees;
  /** Shared lean azimuth: 0 degrees is +X, 90 degrees is +Z. */
  //@inspect slider min=0 max=360
  float tilt_direction_degrees;
  //@end_group

  //@begin_group "Wind"
  /** Wind azimuth: 0 degrees is +X, 90 degrees is +Z. */
  //@inspect slider min=0 max=360
  float wind_direction_degrees;
  //@inspect slider min=0 max=4
  float wind_force;
  /** Animation cycles per second. */
  //@inspect slider min=0 max=5
  float wind_speed;
  //@end_group

  //@begin_group "Noise"
  /** Grayscale noise image used for world-space color and height sampling. */
  LDKAssetImage color_noise_texture;
  /** Noise repetitions per world unit. Must be positive. */
  //@inspect min=0.0001
  float color_noise_scale;
  /** Color stops, mapped to the noise [0, 1] interval. */
  //@inspect slider min=0 max=1
  float color_stop_1_position;
  //@inspect slider min=0 max=1
  float color_stop_2_position;
  //@inspect widget=COLOR
  u32 color_stop_0_color;
  //@inspect widget=COLOR
  u32 color_stop_1_color;
  //@inspect widget=COLOR
  u32 color_stop_2_color;
  //@inspect widget=COLOR
  u32 color_stop_3_color;
  //@end_group

  //@begin_group "Material"
  //@inspect slider min=0 max=1
  float specular;
  //@inspect slider min=1 max=256
  float shininess;
  //@inspect slider min=0 max=4
  float emission;
  bool casts_shadows;
  bool unlit;
  //@end_group

  //@begin_group "Grass Types"
  /* Array indices are persistent GrassTypeIds; existing slots must not move. */
  //@inspect element_type=LDKGrassTypeDesc initializer=ldk_grass_type_desc_initialize max_count=16 stable_indices
  XArray_LDKGrassTypeDesc *types;
  //@end_group

} LDKGrassSystem;

/** Initialize a useful default grass type description.
 * @param out_desc Receives the descriptor.
 * @group ldk.system.grass_system
 */
LDK_API void ldk_grass_type_desc_defaults(LDKGrassTypeDesc *out_desc);

/** Default initializer for Comet-created grass array elements. */
LDK_API void ldk_grass_type_desc_initialize(void *element);

/** Return the active configuration index for name, or INVALID when absent.
 * @param name Name identifying the object or setting.
 * @return The requested value.
 * @group ldk.system.grass_system
 */
LDK_API LDKGrassTypeId ldk_grass_get_id_by_name(const char *name);

/** True when type_id names a configured slot in the active GrassSystem.
 * @param type_id Identifier of the type.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_type_is_valid(LDKGrassTypeId type_id);

/** Copy one configured type from the active GrassSystem.
 * @param type_id Identifier of the type.
 * @param out_desc Receives the descriptor.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_type_get(
    LDKGrassTypeId type_id, LDKGrassTypeDesc *out_desc);

/**
 * Register an additional type in the first free runtime slot. Names must be
 * unique. Returns INVALID when the system is inactive, the description is
 * invalid, the name already exists, or all 16 slots are occupied.
 * @param desc Description or configuration for this operation.
 * @return The requested value.
 * @group ldk.system.grass_system
 */
LDK_API LDKGrassTypeId ldk_grass_type_register(
    const LDKGrassTypeDesc *desc);

/** Initialize an empty one-square-unit patch with density scale 1.
 * @param out_desc Receives the descriptor.
 * @group ldk.system.grass_system
 */
LDK_API void ldk_grass_patch_desc_defaults(LDKGrassPatchDesc *out_desc);

/** Return an invalid grass patch handle.
 * @return An invalid or empty handle.
 * @group ldk.system.grass_system
 */
LDK_API LDKGrassPatch ldk_grass_patch_null(void);

/** True while patch identifies a live entry in the active GrassSystem.
 * @param patch Patch used by this operation.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_patch_is_valid(LDKGrassPatch patch);

/** Generate and retain one patch using a configured grass type.
 * @param type_id Identifier of the type.
 * @param desc Description or configuration for this operation.
 * @return The requested value.
 * @group ldk.system.grass_system
 */
LDK_API LDKGrassPatch ldk_grass_add(
    LDKGrassTypeId type_id, const LDKGrassPatchDesc *desc);

/** Replace a patch recipe and regenerate its persistent blade transforms.
 * @param patch Patch used by this operation.
 * @param type_id Identifier of the type.
 * @param desc Description or configuration for this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_update(LDKGrassPatch patch,
    LDKGrassTypeId type_id, const LDKGrassPatchDesc *desc);

/** Remove a patch and every opaque blade belonging to it.
 * @param patch Patch used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_remove(LDKGrassPatch patch);

/** Remove every live patch. Renderer-side style resources remain cached.
 * @group ldk.system.grass_system
 */
LDK_API void ldk_grass_clear(void);

/**
 * Queue one world-space grass interaction capsule for the active GrassSystem.
 * Passing the same point for from and to produces a circular interaction.
 * The queued interaction is consumed by the GrassSystem render update.
 * @param from World-space start of the interaction segment.
 * @param to World-space end of the interaction segment.
 * @param radius Interaction or geometry radius.
 * @param strength Strength of the interaction.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.system.grass_system
 */
LDK_API bool ldk_grass_interact(
    Vec3 from, Vec3 to, float radius, float strength);

#ifdef __cplusplus
}
#endif

#endif // LDK_GRASS_H
