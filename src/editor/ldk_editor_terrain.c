/* Built-in TerrainComponent inspector and scene-view sculpting. The scene
 * owns the editable height data; imported source images are never modified. */
#include "ldk_editor_atlas.h"
#include "ldk_editor_internal.h"
#include "ldk_editor_ui.h"

#include <component/ldk_camera.h>
#include <component/ldk_terrain_component.h>
#include <component/ldk_transform.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>
#include <ldk_debug_draw.h>
#include <ldk_image.h>
#include <ldk_os.h>
#include <module/ldk_asset_source.h>
#include <module/ldk_asset_manager.h>
#include <stdx/stdx_filesystem.h>

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TERRAIN_UNDO_TAG UINT32_C(0x5452524e)
#define TERRAIN_BRUSH_SEGMENTS 48u
#define TERRAIN_PI 3.14159265358979323846f

typedef struct LDKEditorTerrainUndoRegion
{
  u32 x, y, width, height;
  u32 pixel_size;
  /* Sculpt (0) stores RGBA16 pixels and height range; vegetation (1)
   * stores R8 density samples addressed by stable layer ID. */
  u32 kind;
  u64 layer_id;
  LDKEditorTerrainHeightSnapshot range_state;
  /* Region pixels follow, in row-major order. */
} LDKEditorTerrainUndoRegion;

static void s_terrain_stroke_finish(LDKEditorContext *editor);

/* Editor-only presentation state, keyed by terrain entity and stable layer ID.
 * Fold state and in-progress text input never become terrain/scene data. */
typedef struct LDKEditorTerrainVegetationFold
{
  u64 layer_id;
  bool expanded;
  bool editing_name;
  char name_text[X_SMALLSTR_MAX_LENGTH + 1u];
} LDKEditorTerrainVegetationFold;

typedef struct LDKEditorTerrainVegetationFolds
{
  LDKEntity owner;
  bool initialized;
  bool expanded;
  u32 count;
  LDKEditorTerrainVegetationFold
      layers[LDK_TERRAIN_VEGETATION_LAYER_COUNT];
} LDKEditorTerrainVegetationFolds;

static LDKEditorTerrainVegetationFolds s_terrain_vegetation_folds;

/* Surface slots have positional semantics: unlike vegetation layers, terrain
 * surfaces do not have persistent IDs. Fold state follows the slot on edit
 * and explicitly follows the element on move/delete. Nothing is serialized. */
typedef struct LDKEditorTerrainSurfaceFold
{
  bool expanded;
  bool editing_name;
  bool editing_sprite;
  char name_text[LDK_TERRAIN_LAYER_NAME_CAPACITY];
  char sprite_text[32];
} LDKEditorTerrainSurfaceFold;

typedef struct LDKEditorTerrainSurfaceFolds
{
  LDKEntity owner;
  bool initialized;
  bool expanded;
  LDKEditorTerrainSurfaceFold surfaces[LDK_TERRAIN_SURFACE_COUNT];
} LDKEditorTerrainSurfaceFolds;

static LDKEditorTerrainSurfaceFolds s_terrain_surface_folds;


static LDKUIIcon s_terrain_ui_icon(LDKEditorContext *editor,
    LDKEditorIcon icon_id)
{
  LDKUIIcon icon = {0};
  icon.color_role = LDK_UI_ICON_COLOR_DEFAULT;
  icon.texture = ldk_renderer_texture_ui_handle(
      editor->renderer, editor->ui_atlas);
  icon.size.w = icon.size.h = LDK_UI_DEFAULT_CONTROL_HEIGHT;
  icon.uv = ldk_editor_icon_rects[icon_id];
  return icon;
}


static size_t s_terrain_bytes(const LDKEditorTerrainState *state)
{
  return (size_t)state->data.width * state->data.height * 4u *
      (state->image_format == LDK_IMAGE_FORMAT_RGBA16_UNORM ? 2u : 1u);
}

static LDKImageInfo s_terrain_info(const LDKEditorTerrainState *state)
{
  LDKImageInfo info = {0};
  info.width = state->data.width;
  info.height = state->data.height;
  info.channel_count = 4u;
  info.bytes_per_channel = state->image_format ==
      LDK_IMAGE_FORMAT_RGBA16_UNORM ? 2u : 1u;
  info.format = state->image_format;
  info.byte_count = state->pixel_bytes;
  info.pixels = state->pixels;
  return info;
}

static LDKEditorTerrainHeightSnapshot s_terrain_height_snapshot(
    const LDKTerrain *terrain)
{
  LDKEditorTerrainHeightSnapshot result = {0};
  result.height_scale = terrain->height_scale;
  result.height_offset = terrain->height_offset;
  for (u32 i = 0u; i < terrain->surface_count; ++i)
  {
    result.surface_min_height[i] = terrain->surfaces[i].min_height;
    result.surface_blend_width[i] = terrain->surfaces[i].blend_width;
  }
  return result;
}

static void s_terrain_height_restore(LDKTerrain *terrain,
    const LDKEditorTerrainHeightSnapshot *snapshot)
{
  terrain->height_scale = snapshot->height_scale;
  terrain->height_offset = snapshot->height_offset;
  for (u32 i = 0u; i < terrain->surface_count; ++i)
  {
    terrain->surfaces[i].min_height = snapshot->surface_min_height[i];
    terrain->surfaces[i].blend_width = snapshot->surface_blend_width[i];
  }
}

static LDKTerrainComponent *s_terrain_owned_component(
    const LDKEditorTerrainState *state)
{
  if (!state || x_handle_is_null(state->owner_entity)) return NULL;
  return ldk_ecs_component_get(state->owner_entity,
      LDK_COMPONENT_TYPE_TERRAIN);
}

/* Switch the editable mask without modifying the component. Missing masks
 * start empty, so painting a new layer never seeds unrequested grass. */
static bool s_terrain_vegetation_select(LDKEditorContext *editor,
    u64 layer_id)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  const LDKTerrainComponent *component = s_terrain_owned_component(state);
  if (!component || !state->loaded) return false;
  const u64 sample_count = (u64)state->data.width * state->data.height;
  if (!sample_count || sample_count > SIZE_MAX) return false;
  bool found = false;
  for (u32 i = 0u; i < state->data.vegetation_layer_count; ++i)
    if (state->data.vegetation_layers[i].id == layer_id)
      {found = true; break;}
  if (!found) return false;
  u8 *pixels = calloc((size_t)sample_count, 1u);
  if (!pixels) return false;
  const u8 *existing = ldk_terrain_component_vegetation_density_get(
      component, layer_id);
  if (existing) memcpy(pixels, existing, (size_t)sample_count);
  free(state->vegetation_pixels);
  state->vegetation_pixels = pixels;
  state->vegetation_layer_id = layer_id;
  return true;
}

static bool s_terrain_publish(LDKEditorContext *editor)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  LDKTerrainComponent *component = s_terrain_owned_component(state);
  if (!state->loaded || !component || !state->pixels) return false;
  LDKImageInfo image = s_terrain_info(state);
  if (!ldk_terrain_component_set(component, &state->data, &image)) return false;
  state->observed_revision = component->revision;
  ldki_editor_scene_dirty_set(editor, true);
  return true;
}

static void s_terrain_undo_discard(LDKEditorContext *editor)
{
  const LDKEditorTerrainState *state = &editor->terrain_editor;
  if (state->loaded)
    ldk_editor_undo_discard_component(&editor->undo,
        state->undo_owner, TERRAIN_UNDO_TAG);
}

static void s_terrain_fields_sync(LDKEditorTerrainState *state)
{
  snprintf(state->width_text, sizeof(state->width_text),
      "%u", state->target_width);
  snprintf(state->depth_text, sizeof(state->depth_text),
      "%u", state->target_height);
  snprintf(state->cell_size_text, sizeof(state->cell_size_text),
      "%.6g", (double)state->target_cell_size);
  /* The Inspector edits the world-space maximum, not the encoded range.
   * Samples remain normalized; their elevation is offset + sample * scale. */
  snprintf(state->height_range_text, sizeof(state->height_range_text),
      "%.6g", (double)(state->data.height_offset + state->height_range));
  snprintf(state->flatten_height_text, sizeof(state->flatten_height_text),
      "%.6g", (double)state->flatten_height);
}

/* Switching selection releases only the working brush copy. Persisted
 * terrain data and entity Undo history remain owned by the scene. */
static void s_terrain_reset(LDKEditorContext *editor, bool unused)
{
  (void)unused;
  LDKEditorTerrainState *state = &editor->terrain_editor;
  free(state->pixels);
  free(state->vegetation_pixels);
  free(state->stroke_start);
  memset(state, 0, sizeof(*state));
  state->target_width = 256u;
  state->target_height = 256u;
  state->target_cell_size = 1.0f;
  state->height_range = 64.0f;
  state->flatten_height = 0.0f;
  s_terrain_fields_sync(state);
  state->radius = 8.0f;
  state->strength = 8.0f;
  state->falloff = 0.65f;
  state->image_format = LDK_IMAGE_FORMAT_RGBA16_UNORM;
  state->owner_entity = x_handle_null();
  state->selection_attempt = x_handle_null();
}

static u64 s_terrain_undo_owner(LDKEntity entity)
{
  return ((u64)entity.version << 32u) | entity.index;
}

static bool s_terrain_bind(LDKEditorContext *editor, LDKEntity entity,
    const LDKTerrainComponent *component)
{
  if (!component || !ldk_terrain_validate(&component->terrain)) return false;
  LDKImageInfo source = {0};
  if (component->heightmap &&
      (!ldk_image_get_info(component->heightmap, &source) ||
       source.width != component->terrain.width ||
       source.height != component->terrain.height)) return false;
  const u64 sample_count = (u64)component->terrain.width *
      component->terrain.height;
  if (!sample_count || sample_count > SIZE_MAX / (4u * sizeof(u16)))
    return false;
  const size_t bytes = (size_t)sample_count * 4u * sizeof(u16);
  u8 *pixels = calloc(1u, (size_t)bytes);
  if (!pixels) return false;
  if (component->heightmap)
  {
    if (source.format == LDK_IMAGE_FORMAT_RGBA16_UNORM)
      memcpy(pixels, source.pixels, (size_t)bytes);
    else if (source.format == LDK_IMAGE_FORMAT_RGBA8_UNORM)
    {
      for (size_t i = 0u; i < (size_t)bytes / sizeof(u16); ++i)
      {
        const u16 value = (u16)source.pixels[i] * 257u;
        memcpy(pixels + i * sizeof(u16), &value, sizeof(value));
      }
    }
    else
    {
      free(pixels);
      return false;
    }
  }
  LDKEditorTerrainState *state = &editor->terrain_editor;
  state->data = component->terrain;
  state->pixels = pixels;
  state->pixel_bytes = (size_t)bytes;
  state->image_format = LDK_IMAGE_FORMAT_RGBA16_UNORM;
  state->target_width = state->data.width;
  state->target_height = state->data.height;
  state->target_cell_size = state->data.cell_size;
  state->height_range = state->data.height_scale;
  state->flatten_height = state->data.height_offset;
  state->owner_entity = entity;
  state->selection_attempt = entity;
  state->undo_owner = s_terrain_undo_owner(entity);
  state->observed_revision = component->revision;
  state->loaded = true;
  s_terrain_fields_sync(state);
  if (state->data.vegetation_layer_count)
    (void)s_terrain_vegetation_select(editor,
        state->data.vegetation_layers[0].id);
  return true;
}

static void s_terrain_selection_sync(LDKEditorContext *editor)
{
  if (!editor) return;
  LDKEditorTerrainState *state = &editor->terrain_editor;
  LDKEntity selected = editor->selected_entity;
  const LDKTerrainComponent *component = !x_handle_is_null(selected)
      ? ldk_ecs_component_get_const(selected, LDK_COMPONENT_TYPE_TERRAIN)
      : NULL;
  if (!component || !ldk_ecs_component_is_enabled(selected,
          LDK_COMPONENT_TYPE_TERRAIN))
  {
    if (state->stroke_active) s_terrain_stroke_finish(editor);
    if (state->loaded) s_terrain_reset(editor, false);
    return;
  }
  if (state->loaded && ldki_editor_entity_equal(state->owner_entity, selected) &&
      state->observed_revision == component->revision) return;
  if (state->stroke_active) s_terrain_stroke_finish(editor);
  s_terrain_reset(editor, false);
  (void)s_terrain_bind(editor, selected, component);
}

static void s_terrain_ui_error(
    LDKEditorContext *editor, const char *message)
{
  ldki_editor_log_error(editor, message);
  (void)ldk_os_dialog_show_error(editor->window,
      "Terrain Editor", message);
}

static bool s_terrain_entity_world(LDKEditorContext *editor,
    Mat4 *world, Mat4 *inverse)
{
  const LDKEditorTerrainState *state = &editor->terrain_editor;
  if (x_handle_is_null(state->owner_entity) ||
      !ldk_ecs_component_get_const(
          state->owner_entity, LDK_COMPONENT_TYPE_TERRAIN) ||
      !ldk_ecs_component_is_enabled(
          state->owner_entity, LDK_COMPONENT_TYPE_TERRAIN) ||
      !ldk_scenegraph_update_entity(state->owner_entity) ||
      !ldk_transform_get_world_matrix(state->owner_entity, world))
    return false;
  bool inverse_ok = false;
  *inverse = mat4_inverse_full(*world, &inverse_ok);
  return inverse_ok;
}

/* Export is deliberately independent of Save Scene, and reads the currently
 * sculpted component pixels without modifying the component. */
static bool s_terrain_export_heightmap(LDKEditorContext *editor)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  if (!state->loaded || !state->pixels) return false;
  XFSPath output = {0};
  if (!ldk_os_dialog_show_save_file(editor->window,
          "Export Heightmap", "PNG (*.png)\0*.png\0", &output))
    return false;
  const char *extension = strrchr(output.buf, '.');
  if (!extension || (strcmp(extension, ".png") != 0 &&
          strcmp(extension, ".PNG") != 0))
  {
    s_terrain_ui_error(editor, "Export Heightmap requires a .png filename.");
    return false;
  }
  const u64 sample_count = (u64)state->data.width * state->data.height;
  if (sample_count > SIZE_MAX / (4u * sizeof(u16))) return false;
  u16 *pixels = malloc((size_t)sample_count * 4u * sizeof(u16));
  if (!pixels) return false;
  const u32 channel = (u32)state->data.height_channel;
  for (u64 i = 0u; i < sample_count; ++i)
  {
    u16 value = 0u;
    const size_t source = (size_t)i * 4u + channel;
    if (state->image_format == LDK_IMAGE_FORMAT_RGBA16_UNORM)
      memcpy(&value, state->pixels + source * sizeof(u16), sizeof(value));
    else
      value = (u16)state->pixels[source] * 257u;
    for (u32 k = 0u; k < 3u; ++k) pixels[(size_t)i * 4u + k] = value;
    pixels[(size_t)i * 4u + 3u] = 65535u;
  }
  LDKImage *image = ldk_image_create_format(state->data.width,
      state->data.height, LDK_IMAGE_FORMAT_RGBA16_UNORM, pixels);
  free(pixels);
  if (!image) return false;
  const bool ok = ldk_image_save_png(image, output.buf);
  ldk_image_destroy(image);
  return ok;
}

/* Initialize or replace the component's own heightfield without requiring
 * an external image or creating a separate terrain asset. */
static bool s_terrain_create_flat(LDKEditorContext *editor)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  if (!state->loaded || state->stroke_active ||
      !state->target_width || !state->target_height ||
      !isfinite(state->target_cell_size) || state->target_cell_size <= 0.0f ||
      !isfinite(state->height_range) || state->height_range <= 0.0f)
    return false;
  LDKTerrain terrain = state->data;
  terrain.width = state->target_width;
  terrain.height = state->target_height;
  terrain.cell_size = state->target_cell_size;
  terrain.height_scale = state->height_range;
  terrain.height_offset = 0.0f;
  if (!ldk_terrain_validate(&terrain)) return false;
  const u64 sample_count = (u64)terrain.width * terrain.height;
  if (!sample_count || sample_count > SIZE_MAX / 8u) return false;
  const size_t bytes = (size_t)sample_count * 8u;
  u8 *pixels = calloc(1u, (size_t)bytes);
  if (!pixels) return false;
  /* Flat terrain needs no serialized samples. Keep a zeroed scratch grid
   * for sculpting; the first brush stroke publishes the RGBA16 image. */
  LDKTerrainComponent *component = s_terrain_owned_component(state);
  if (!component || !ldk_terrain_component_set(component, &terrain, NULL))
  {
    free(pixels);
    return false;
  }
  s_terrain_undo_discard(editor);
  free(state->pixels);
  free(state->stroke_start);
  state->stroke_start = NULL;
  state->pixels = pixels;
  state->pixel_bytes = (size_t)bytes;
  state->image_format = LDK_IMAGE_FORMAT_RGBA16_UNORM;
  state->data = terrain;
  state->observed_revision = component->revision;
  state->dirty = true;
  state->pixels_dirty = true;
  state->stroke_active = false;
  if (state->vegetation_layer_id)
    (void)s_terrain_vegetation_select(editor, state->vegetation_layer_id);
  s_terrain_fields_sync(state);
  ldki_editor_scene_dirty_set(editor, true);
  return true;
}

static bool s_terrain_import(LDKEditorContext *editor)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  XFSPath path = {0};
  LDKImage *source = NULL;
  LDKImage *scaled = NULL;
  LDKImageInfo info = {0};
  LDKTerrain imported;
  u8 *pixels = NULL;
  bool success = false;
  if (!state->loaded || !state->target_width || !state->target_height ||
      !isfinite(state->target_cell_size) || state->target_cell_size <= 0.0f)
    return false;
  if (!ldk_os_dialog_show_open_file(editor->window,
          "Import Heightmap", "Image Files\0*.png;*.tga;*.bmp\0", &path))
    return false;
  source = ldk_image_load(path.buf);
  if (!source) goto done;
  scaled = ldk_terrain_heightmap_resample(source,
      state->target_width, state->target_height);
  if (!scaled || !ldk_image_get_info(scaled, &info) ||
      !ldk_terrain_import_heightmap(&imported, NULL, scaled,
          LDK_TERRAIN_CHANNEL_R)) goto done;

  LDKTerrain candidate = state->data;
  candidate.heightmap_path = imported.heightmap_path;
  candidate.height_channel = imported.height_channel;
  candidate.width = imported.width;
  candidate.height = imported.height;
  candidate.cell_size = state->target_cell_size;
  candidate.height_scale = state->height_range;
  if (!ldk_terrain_validate(&candidate) || info.byte_count > SIZE_MAX)
    goto done;
  pixels = malloc((size_t)info.byte_count);
  if (!pixels) goto done;
  memcpy(pixels, info.pixels, (size_t)info.byte_count);
  /* Publish the complete replacement atomically. On failure the component
   * and working copy remain unchanged. */
  LDKTerrainComponent *component = s_terrain_owned_component(state);
  if (!component || !ldk_terrain_component_set(component, &candidate, &info))
    goto done;
  s_terrain_undo_discard(editor); /* Grid size changed: old regions are invalid. */
  free(state->pixels);
  free(state->stroke_start);
  state->stroke_start = NULL;
  state->pixels = pixels;
  state->pixel_bytes = (size_t)info.byte_count;
  state->image_format = info.format;
  state->data = candidate;
  state->observed_revision = component->revision;
  state->dirty = true;
  state->pixels_dirty = true;
  state->stroke_active = false;
  if (state->vegetation_layer_id)
    (void)s_terrain_vegetation_select(editor, state->vegetation_layer_id);
  ldki_editor_scene_dirty_set(editor, true);
  pixels = NULL;
  success = true;
done:
  free(pixels);
  ldk_image_destroy(scaled);
  ldk_image_destroy(source);
  return success;
}

/* Undo payloads cover only the changed bounding rectangle, not the full
 * heightmap. The scratch snapshot is freed after each mouse stroke. */
static void s_terrain_stroke_finish(LDKEditorContext *editor)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  if (!state->stroke_active) return;
  state->stroke_active = false;
  const bool vegetation = state->stroke_layer_id != 0u;
  const u8 *current = vegetation ? state->vegetation_pixels : state->pixels;
  if (!state->stroke_start || !current) goto done;
  const u32 width = state->data.width, height = state->data.height;
  const u32 pixel_size = vegetation ? 1u :
      (state->image_format == LDK_IMAGE_FORMAT_RGBA16_UNORM ? 8u : 4u);
  u32 min_x = width, min_y = height, max_x = 0u, max_y = 0u;
  for (u32 y = 0u; y < height; ++y)
  {
    for (u32 x = 0u; x < width; ++x)
    {
      size_t at = ((size_t)y * width + x) * pixel_size;
      if (memcmp(current + at, state->stroke_start + at,
              pixel_size) != 0)
      {
        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (x > max_x) max_x = x;
        if (y > max_y) max_y = y;
      }
    }
  }
  const LDKEditorTerrainHeightSnapshot after_height =
      s_terrain_height_snapshot(&state->data);
  if (min_x == width)
  {
    if (vegetation || memcmp(&after_height, &state->stroke_height_start,
            sizeof(after_height)) == 0) goto done;
    /* A metadata-only change still needs an Undo region header. */
    min_x = min_y = max_x = max_y = 0u;
  }
  const u32 rw = max_x - min_x + 1u, rh = max_y - min_y + 1u;
  const size_t pitch = (size_t)rw * pixel_size;
  const size_t bytes = sizeof(LDKEditorTerrainUndoRegion) + pitch * rh;
  u8 *before = malloc(bytes), *after = malloc(bytes);
  if (!before || !after) goto done_regions;
  LDKEditorTerrainUndoRegion header = {0};
  header.x = min_x;
  header.y = min_y;
  header.width = rw;
  header.height = rh;
  header.pixel_size = pixel_size;
  header.kind = vegetation ? 1u : 0u;
  header.layer_id = state->stroke_layer_id;
  header.range_state = state->stroke_height_start;
  memcpy(before, &header, sizeof(header));
  header.range_state = after_height;
  memcpy(after, &header, sizeof(header));
  for (u32 y = 0u; y < rh; ++y)
  {
    size_t start = ((size_t)(min_y + y) * width + min_x) * pixel_size;
    memcpy(before + sizeof(header) + (size_t)y * pitch,
        state->stroke_start + start, pitch);
    memcpy(after + sizeof(header) + (size_t)y * pitch,
        current + start, pitch);
  }
  LDKEditorUndoKey key = {state->undo_owner,
      ((u64)TERRAIN_UNDO_TAG << 32u) | 1u};
  if (ldk_editor_undo_begin_change(&editor->undo, key,
          LDK_FIELD_ARRAY, before, bytes))
    (void)ldk_editor_undo_end_change(&editor->undo, after);
 done_regions:
  free(before);
  free(after);
 done:
  free(state->stroke_start);
  state->stroke_start = NULL;
  state->stroke_layer_id = 0u;
}

bool ldki_editor_terrain_undo_is_key(LDKEditorUndoKey key)
{
  return (u32)(key.property >> 32u) == TERRAIN_UNDO_TAG;
}

static LDKTerrainComponent *s_terrain_undo_component(LDKEditorUndoKey key)
{
  LDKEntity entity = {(u32)key.owner, (u32)(key.owner >> 32u)};
  return ldk_ecs_component_get(entity, LDK_COMPONENT_TYPE_TERRAIN);
}

void *ldki_editor_terrain_undo_resolve(LDKEditorContext *editor,
    LDKEditorUndoKey key, LDKFieldType type, size_t size)
{
  (void)editor;
  if (type != LDK_FIELD_ARRAY ||
      size < sizeof(LDKEditorTerrainUndoRegion)) return NULL;
  return s_terrain_undo_component(key);
}

bool ldki_editor_terrain_undo_apply(LDKEditorContext *editor,
    LDKEditorUndoKey key, LDKFieldType type, const void *bytes, size_t size)
{
  LDKTerrainComponent *component = s_terrain_undo_component(key);
  if (!bytes || !component ||
      !ldki_editor_terrain_undo_resolve(editor, key, type, size))
    return false;
  LDKEditorTerrainUndoRegion region;
  memcpy(&region, bytes, sizeof(region));
  if (!region.width || !region.height ||
      region.x >= component->terrain.width ||
      region.y >= component->terrain.height ||
      region.width > component->terrain.width - region.x ||
      region.height > component->terrain.height - region.y ||
      (region.kind > 1u ||
       region.pixel_size != (region.kind == 1u ? 1u : 8u)) ||
      (u64)region.width * region.height * region.pixel_size >
          SIZE_MAX - sizeof(region) ||
      size != sizeof(region) + (size_t)region.width * region.height *
          region.pixel_size)
    return false;
  if (region.kind == 1u)
  {
    if (!region.layer_id || !ldk_terrain_validate(&component->terrain))
      return false;
    bool layer_exists = false;
    for (u32 i = 0u; i < component->terrain.vegetation_layer_count; ++i)
      if (component->terrain.vegetation_layers[i].id == region.layer_id)
        {layer_exists = true; break;}
    if (!layer_exists) return false;
    const u64 count = (u64)component->terrain.width *
        component->terrain.height;
    if (!count || count > SIZE_MAX) return false;
    u8 *pixels = calloc((size_t)count, 1u);
    if (!pixels) return false;
    const u8 *old = ldk_terrain_component_vegetation_density_get(
        component, region.layer_id);
    if (old) memcpy(pixels, old, (size_t)count);
    const u8 *source = (const u8 *)bytes + sizeof(region);
    for (u32 z = 0u; z < region.height; ++z)
      memcpy(pixels + (size_t)(region.y + z) * component->terrain.width +
              region.x, source + (size_t)z * region.width, region.width);
    bool ok = ldk_terrain_component_vegetation_density_set(
        component, region.layer_id, pixels);
    if (ok)
    {
      LDKEditorTerrainState *state = &editor->terrain_editor;
      if (state->loaded &&
          s_terrain_undo_owner(state->owner_entity) == key.owner)
      {
        state->observed_revision = component->revision;
        if (state->vegetation_layer_id == region.layer_id)
        {
          free(state->vegetation_pixels);
          state->vegetation_pixels = pixels;
          pixels = NULL;
        }
      }
      ldki_editor_scene_dirty_set(editor, true);
    }
    free(pixels);
    return ok;
  }
  LDKTerrain candidate = component->terrain;
  s_terrain_height_restore(&candidate, &region.range_state);
  if (!ldk_terrain_validate(&candidate)) return false;
  const u64 sample_count = (u64)candidate.width * candidate.height;
  if (!sample_count || sample_count > SIZE_MAX / 8u) return false;
  const size_t pixel_bytes = (size_t)sample_count * 8u;
  u8 *pixels = calloc(1u, pixel_bytes);
  if (!pixels) return false;
  LDKImageInfo previous = {0};
  if (component->heightmap)
  {
    if (!ldk_image_get_info(component->heightmap, &previous) ||
        previous.format != LDK_IMAGE_FORMAT_RGBA16_UNORM ||
        previous.byte_count != pixel_bytes)
    {
      free(pixels);
      return false;
    }
    memcpy(pixels, previous.pixels, pixel_bytes);
  }
  const size_t pitch = (size_t)region.width * region.pixel_size;
  const u8 *source = (const u8 *)bytes + sizeof(region);
  for (u32 y = 0u; y < region.height; ++y)
  {
    const size_t offset = ((size_t)(region.y + y) * candidate.width +
        region.x) * region.pixel_size;
    memcpy(pixels + offset, source + (size_t)y * pitch, pitch);
  }
  LDKImageInfo info = {candidate.width, candidate.height, 4u, 2u,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, pixel_bytes, pixels};
  bool ok = ldk_terrain_component_set(component, &candidate, &info);
  free(pixels);
  if (!ok) return false;
  LDKEditorTerrainState *state = &editor->terrain_editor;
  if (state->loaded && s_terrain_undo_owner(state->owner_entity) == key.owner)
  {
    /* Refresh the working buffer without exiting Sculpt or resetting the
     * user's tool settings after Undo/Redo. */
    const bool sculpt_enabled = state->enabled;
    const LDKEditorTerrainSculptTool tool = state->sculpt_tool;
    const float radius = state->radius;
    const float strength = state->strength;
    const float falloff = state->falloff;
    const float flatten_height = state->flatten_height;
    const u64 layer_id = state->vegetation_layer_id;
    const LDKEditorTerrainEditMode edit_mode = state->edit_mode;
    const bool vegetation_erase = state->vegetation_erase;
    char vegetation_type_text[LDK_TERRAIN_LAYER_NAME_CAPACITY];
    memcpy(vegetation_type_text, state->vegetation_type_text,
        sizeof(vegetation_type_text));
    s_terrain_reset(editor, false);
    (void)s_terrain_bind(editor, editor->selected_entity, component);
    state->enabled = sculpt_enabled;
    state->sculpt_tool = tool;
    state->radius = radius;
    state->strength = strength;
    state->falloff = falloff;
    state->flatten_height = flatten_height;
    state->edit_mode = edit_mode;
    state->vegetation_erase = vegetation_erase;
    memcpy(state->vegetation_type_text, vegetation_type_text,
        sizeof(vegetation_type_text));
    if (layer_id && state->vegetation_layer_id != layer_id)
      (void)s_terrain_vegetation_select(editor, layer_id);
    s_terrain_fields_sync(state);
  }
  ldki_editor_scene_dirty_set(editor, true);
  return true;
}

static bool s_terrain_stroke_begin(LDKEditorContext *editor)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  if (state->stroke_active || !state->pixels) return false;
  if (ldk_editor_undo_has_active_change(&editor->undo))
  {
    if (!ldk_editor_undo_end_change_resolved(&editor->undo)) return false;
    editor->undo_widget_id = 0;
    editor->undo_end_requested = false;
  }
  const bool vegetation = state->edit_mode ==
      LDK_EDITOR_TERRAIN_EDIT_VEGETATION;
  const size_t bytes = vegetation
      ? (size_t)state->data.width * state->data.height
      : state->pixel_bytes;
  const u8 *pixels = vegetation ? state->vegetation_pixels : state->pixels;
  if (!pixels) return false;
  state->stroke_start = malloc(bytes);
  if (!state->stroke_start) return false;
  memcpy(state->stroke_start, pixels, bytes);
  state->stroke_layer_id = vegetation ? state->vegetation_layer_id : 0u;
  state->stroke_height_start = s_terrain_height_snapshot(&state->data);
  /* Flatten uses the explicit inspector target throughout the stroke. */
  state->stroke_active = true;
  return true;
}

static float s_terrain_clamp01(float x)
{
  return fminf(1.0f, fmaxf(0.0f, x));
}

/* Brush editing always uses the configured height channel. The other
 * channels must remain intact for imported images and Undo/Redo. */
static u16 s_terrain_brush_sample(const LDKEditorTerrainState *state,
    const u8 *pixels, u32 x, u32 z)
{
  const size_t index = ((size_t)z * state->data.width + x) * 4u +
      (u32)state->data.height_channel;
  if (state->image_format == LDK_IMAGE_FORMAT_RGBA16_UNORM)
  {
    u16 value;
    memcpy(&value, pixels + index * sizeof(value), sizeof(value));
    return value;
  }
  return (u16)pixels[index] * 257u;
}

static bool s_terrain_brush_store(LDKEditorTerrainState *state,
    u32 x, u32 z, float normalized)
{
  const size_t index = ((size_t)z * state->data.width + x) * 4u +
      (u32)state->data.height_channel;
  normalized = s_terrain_clamp01(normalized);
  if (state->image_format == LDK_IMAGE_FORMAT_RGBA16_UNORM)
  {
    const u16 value = (u16)lroundf(normalized * 65535.0f);
    u16 previous;
    memcpy(&previous, state->pixels + index * sizeof(value),
        sizeof(previous));
    if (previous == value) return false;
    memcpy(state->pixels + index * sizeof(value), &value, sizeof(value));
    return true;
  }
  const u8 value = (u8)lroundf(normalized * 255.0f);
  if (state->pixels[index] == value) return false;
  state->pixels[index] = value;
  return true;
}

/* Smooth reads an immutable, one-pixel-padded snapshot. Without the snapshot,
 * samples edited earlier in the same dab would bias later ones toward the
 * iteration direction. Only the height channel is copied, not the whole map. */
static bool s_terrain_brush_level_apply(LDKEditorTerrainState *state,
    Vec3 center, float dt, int x0, int z0, int x1, int z1,
    float origin_x, float origin_z)
{
  const LDKTerrain *terrain = &state->data;
  const bool smooth = state->sculpt_tool == LDK_EDITOR_TERRAIN_SMOOTH;
  const u32 start_x = x0 > 0 ? (u32)x0 - 1u : 0u;
  const u32 start_z = z0 > 0 ? (u32)z0 - 1u : 0u;
  const u32 end_x = (u32)x1 + ((u32)x1 + 1u < terrain->width ? 1u : 0u);
  const u32 end_z = (u32)z1 + ((u32)z1 + 1u < terrain->height ? 1u : 0u);
  const size_t copy_width = (size_t)end_x - start_x + 1u;
  const size_t copy_height = (size_t)end_z - start_z + 1u;
  u16 *snapshot = NULL;
  if (smooth)
  {
    if (copy_height > SIZE_MAX / sizeof(u16) / copy_width) return false;
    snapshot = malloc(copy_width * copy_height * sizeof(u16));
    if (!snapshot) return false;
    for (u32 z = start_z; z <= end_z; ++z)
      for (u32 x = start_x; x <= end_x; ++x)
        snapshot[((size_t)z - start_z) * copy_width + x - start_x] =
            s_terrain_brush_sample(state, state->pixels, x, z);
  }

  const float radius = state->radius;
  const float inner = radius * (1.0f - state->falloff);
  const float target = s_terrain_clamp01((state->flatten_height -
      terrain->height_offset) / terrain->height_scale);
  bool changed = false;
  for (int z = z0; z <= z1; ++z)
  {
    for (int x = x0; x <= x1; ++x)
    {
      const float wx = origin_x + ((float)x + 0.5f) * terrain->cell_size;
      const float wz = origin_z + ((float)z + 0.5f) * terrain->cell_size;
      const float distance = sqrtf((wx - center.x) * (wx - center.x) +
          (wz - center.z) * (wz - center.z));
      if (distance >= radius) continue;
      float weight = distance <= inner ? 1.0f :
          s_terrain_clamp01((radius - distance) /
              fmaxf(radius - inner, 0.00001f));
      weight = weight * weight * (3.0f - 2.0f * weight);
      /* For both tools Strength is a relaxation rate per second. A frame
       * never overshoots the neighborhood mean or the captured plane. */
      const float alpha = s_terrain_clamp01(state->strength * dt * weight);
      if (alpha <= 0.0f) continue;
      float previous, desired;
      if (smooth)
      {
        static const u32 kernel[3] = {1u, 2u, 1u};
        const float old_value = (float)snapshot[
            ((size_t)z - start_z) * copy_width + (size_t)x - start_x];
        u32 sum = 0u;
        for (int dz = -1; dz <= 1; ++dz)
        {
          const u32 sz = (u32)(z + dz < 0 ? 0 :
              z + dz >= (int)terrain->height ? (int)terrain->height - 1 :
              z + dz);
          for (int dx = -1; dx <= 1; ++dx)
          {
            const u32 sx = (u32)(x + dx < 0 ? 0 :
                x + dx >= (int)terrain->width ? (int)terrain->width - 1 :
                x + dx);
            const u16 sample = snapshot[
                ((size_t)sz - start_z) * copy_width + sx - start_x];
            sum += kernel[dz + 1] * kernel[dx + 1] * sample;
          }
        }
        previous = old_value / 65535.0f;
        desired = (float)sum / (16.0f * 65535.0f);
      }
      else
      {
        previous = (float)s_terrain_brush_sample(state, state->pixels,
            (u32)x, (u32)z) / 65535.0f;
        desired = target;
      }
      changed |= s_terrain_brush_store(state, (u32)x, (u32)z,
          previous + (desired - previous) * alpha);
    }
  }
  free(snapshot);
  return changed;
}

/* When a brush would exceed the image's [0,1] encoding, enlarge the
 * component's world-height interval and re-encode existing samples. Apply to a
 * scratch copy first so failed asset updates never corrupt the stroke. */
static bool s_terrain_brush_expand_range(LDKEditorContext *editor,
    float minimum, float maximum)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  LDKTerrain candidate = state->data;
  if (minimum >= candidate.height_offset &&
      maximum <= candidate.height_offset + candidate.height_scale)
    return true;
  if (state->image_format != LDK_IMAGE_FORMAT_RGBA16_UNORM)
    return false;

  u8 *copy = malloc(state->pixel_bytes);
  if (!copy) return false;
  memcpy(copy, state->pixels, state->pixel_bytes);
  bool ok = ldk_terrain_heightmap_range_expand(&candidate,
      (u16 *)copy, minimum, maximum);
  if (ok)
  {
    memcpy(state->pixels, copy, state->pixel_bytes);
    state->data = candidate;
    state->height_range = candidate.height_scale;
    s_terrain_fields_sync(state);
    state->dirty = true;
    state->pixels_dirty = true;
  }
  free(copy);
  return ok;
}

/* Paint and erase normalized density at the same world-space positions
 * used by the sculpt brush. Each layer is an independent R8 grid; the user
 * controls spatial coverage, while GrassSystem owns the blade generation. */
static bool s_terrain_vegetation_brush_apply(
    LDKEditorContext *editor, Vec3 center)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  const LDKTerrain *terrain = &state->data;
  if (!state->vegetation_layer_id || !state->vegetation_pixels ||
      state->radius <= 0.0f) return false;
  float ox = terrain->origin_x - (terrain->center_on_origin
      ? (float)terrain->width * terrain->cell_size * 0.5f : 0.0f);
  float oz = terrain->origin_z - (terrain->center_on_origin
      ? (float)terrain->height * terrain->cell_size * 0.5f : 0.0f);
  int x0 = (int)floorf((center.x - state->radius - ox) / terrain->cell_size);
  int z0 = (int)floorf((center.z - state->radius - oz) / terrain->cell_size);
  int x1 = (int)ceilf((center.x + state->radius - ox) / terrain->cell_size);
  int z1 = (int)ceilf((center.z + state->radius - oz) / terrain->cell_size);
  if (x0 < 0) x0 = 0;
  if (z0 < 0) z0 = 0;
  if (x1 >= (int)terrain->width) x1 = (int)terrain->width - 1;
  if (z1 >= (int)terrain->height) z1 = (int)terrain->height - 1;
  if (x1 < x0 || z1 < z0) return false;
  const float inner = state->radius * (1.0f - state->falloff);
  const float dt = fmaxf(0.001f, fminf(0.05f, editor->ui.delta_time));
  const float target = state->vegetation_erase ? 0.0f : 255.0f;
  bool changed = false;
  for (int z = z0; z <= z1; ++z)
    for (int x = x0; x <= x1; ++x)
    {
      const float wx = ox + ((float)x + 0.5f) * terrain->cell_size;
      const float wz = oz + ((float)z + 0.5f) * terrain->cell_size;
      const float distance = sqrtf((wx - center.x) * (wx - center.x) +
          (wz - center.z) * (wz - center.z));
      if (distance >= state->radius) continue;
      float weight = distance <= inner ? 1.0f :
          s_terrain_clamp01((state->radius - distance) /
              fmaxf(state->radius - inner, 0.00001f));
      weight = weight * weight * (3.0f - 2.0f * weight);
      const float alpha = s_terrain_clamp01(state->strength * dt *
          weight * 0.1f);
      size_t index = (size_t)z * terrain->width + (u32)x;
      u8 previous = state->vegetation_pixels[index];
      u8 next = (u8)lroundf(previous +
          (target - previous) * alpha);
      /* R8 quantization must not trap Erase at nonzero density (or Paint
       * below full density). Without this, a weak brush can never remove
       * the final blades: rounding eventually produces the old sample. */
      if (next == previous && alpha > 0.0f &&
          previous != (u8)target)
        next = previous < target ? previous + 1u : previous - 1u;
      if (next != previous)
      {
        state->vegetation_pixels[index] = next;
        changed = true;
      }
    }
  if (!changed) return false;
  LDKTerrainComponent *component = s_terrain_owned_component(state);
  if (!component || !ldk_terrain_component_vegetation_density_set(
          component, state->vegetation_layer_id, state->vegetation_pixels))
    return false;
  state->observed_revision = component->revision;
  state->dirty = true;
  ldki_editor_scene_dirty_set(editor, true);
  return true;
}

static bool s_terrain_brush_apply(LDKEditorContext *editor, Vec3 center)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  const LDKTerrain *terrain = &state->data;
  float origin_x = terrain->origin_x - (terrain->center_on_origin
      ? (float)terrain->width * terrain->cell_size * 0.5f : 0.0f);
  float origin_z = terrain->origin_z - (terrain->center_on_origin
      ? (float)terrain->height * terrain->cell_size * 0.5f : 0.0f);
  const float radius = state->radius;
  if (radius <= 0.0f || !state->pixels || !terrain->height_scale ||
      x_handle_is_null(state->owner_entity)) return false;
  int x0 = (int)floorf((center.x - radius - origin_x) / terrain->cell_size);
  int z0 = (int)floorf((center.z - radius - origin_z) / terrain->cell_size);
  int x1 = (int)ceilf((center.x + radius - origin_x) / terrain->cell_size);
  int z1 = (int)ceilf((center.z + radius - origin_z) / terrain->cell_size);
  if (x0 < 0) x0 = 0;
  if (z0 < 0) z0 = 0;
  if (x1 >= (int)terrain->width) x1 = (int)terrain->width - 1;
  if (z1 >= (int)terrain->height) z1 = (int)terrain->height - 1;
  if (x1 < x0 || z1 < z0) return false;
  const float inner = radius * (1.0f - state->falloff);
  const float dt = fmaxf(0.001f, fminf(0.05f, editor->ui.delta_time));
  if (state->sculpt_tool == LDK_EDITOR_TERRAIN_SMOOTH ||
      state->sculpt_tool == LDK_EDITOR_TERRAIN_FLATTEN)
  {
    bool range_changed = false;
    if (state->sculpt_tool == LDK_EDITOR_TERRAIN_FLATTEN)
    {
      /* The target is a world-space height, not a normalized sample. Let
       * Flatten reach heights beyond the current encoding interval, just
       * as Raise/Lower already do. The stroke's Undo snapshot includes both
       * the expanded interval and the re-encoded pixels. */
      const float min_height = terrain->height_offset;
      const float max_height = min_height + terrain->height_scale;
      range_changed = state->flatten_height < min_height ||
          state->flatten_height > max_height;
      if (range_changed && !s_terrain_brush_expand_range(editor,
              fminf(min_height, state->flatten_height),
              fmaxf(max_height, state->flatten_height)))
        return false;
    }
    const bool changed = s_terrain_brush_level_apply(state, center, dt,
        x0, z0, x1, z1, origin_x, origin_z);
    if ((changed || range_changed) && s_terrain_publish(editor))
    {
      state->dirty = true;
      state->pixels_dirty = true;
    }
    return changed || range_changed;
  }
  const float sign = state->sculpt_tool == LDK_EDITOR_TERRAIN_RAISE
      ? 1.0f : -1.0f;
  /* Find the actual world heights requested by this stroke. A heightmap is
   * normalized storage, not a hard sculpting limit. A component with
   * height_scale=1 must be able to grow beyond one world unit. */
  float needed_min = terrain->height_offset;
  float needed_max = terrain->height_offset + terrain->height_scale;
  for (int z = z0; z <= z1; ++z)
  {
    for (int x = x0; x <= x1; ++x)
    {
      float wx = origin_x + ((float)x + 0.5f) * terrain->cell_size;
      float wz = origin_z + ((float)z + 0.5f) * terrain->cell_size;
      float d = sqrtf((wx - center.x) * (wx - center.x) +
          (wz - center.z) * (wz - center.z));
      if (d >= radius) continue;
      float weight = d <= inner ? 1.0f :
          s_terrain_clamp01((radius - d) /
              fmaxf(radius - inner, 0.00001f));
      weight = weight * weight * (3.0f - 2.0f * weight);
      size_t index = ((size_t)z * terrain->width + (u32)x) * 4u +
          terrain->height_channel;
      float height = 0.0f;
      if (state->image_format == LDK_IMAGE_FORMAT_RGBA16_UNORM)
      {
        u16 value;
        memcpy(&value, state->pixels + index * sizeof(value),
            sizeof(value));
        height = (float)value / 65535.0f;
      }
      else
        height = (float)state->pixels[index] / 255.0f;
      float requested = terrain->height_offset +
          height * terrain->height_scale +
          sign * state->strength * dt * weight;
      if (requested < needed_min) needed_min = requested;
      if (requested > needed_max) needed_max = requested;
    }
  }
  const bool range_changed = needed_min < terrain->height_offset ||
      needed_max > terrain->height_offset + terrain->height_scale;
  if (!s_terrain_brush_expand_range(editor, needed_min, needed_max))
    return false;
  /* s_terrain_brush_expand_range may update state->data in place. The
   * pointer 'terrain' remains valid and now sees the new height interval. */
  bool changed = range_changed;
  for (int z = z0; z <= z1; ++z)
  {
    for (int x = x0; x <= x1; ++x)
    {
      float wx = origin_x + ((float)x + 0.5f) * terrain->cell_size;
      float wz = origin_z + ((float)z + 0.5f) * terrain->cell_size;
      float d = sqrtf((wx - center.x) * (wx - center.x) +
          (wz - center.z) * (wz - center.z));
      if (d >= radius) continue;
      float weight = d <= inner ? 1.0f :
          s_terrain_clamp01((radius - d) / fmaxf(radius - inner, 0.00001f));
      weight = weight * weight * (3.0f - 2.0f * weight);
      size_t index = ((size_t)z * terrain->width + (u32)x) * 4u +
          terrain->height_channel;
      float add = sign * state->strength * dt * weight /
          terrain->height_scale;
      if (state->image_format == LDK_IMAGE_FORMAT_RGBA16_UNORM)
      {
        u16 old_value;
        memcpy(&old_value, state->pixels + index * sizeof(u16),
            sizeof(old_value));
        u16 value = (u16)lroundf(s_terrain_clamp01(
            (float)old_value / 65535.0f + add) * 65535.0f);
        if (value != old_value)
        {
          memcpy(state->pixels + index * sizeof(u16), &value, sizeof(value));
          changed = true;
        }
      }
      else
      {
        u8 old_value = state->pixels[index];
        u8 value = (u8)lroundf(s_terrain_clamp01(
            (float)old_value / 255.0f + add) * 255.0f);
        if (value != old_value)
        {
          state->pixels[index] = value;
          changed = true;
        }
      }
    }
  }
  if (changed && s_terrain_publish(editor))
  {
    state->dirty = true;
    state->pixels_dirty = true;
  }
  return changed;
}

/* Raycast heightfield by intersecting its world AABB and checking the signed
 * distance to the sampled surface. Bracket the first crossing, then bisect. */
static bool s_terrain_ray_hit(LDKEditorContext *editor,
    LDKUIRect rect, LDKUIPoint cursor, Vec3 *out)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  u32 width, height;
  Mat4 view, projection;
  bool inverse_ok;
  if (!ldk_renderer_view_extent_get(editor->renderer,
          editor->scene_view, &width, &height) || !width || !height ||
      !ldk_camera_get_view_matrix(editor->editor_camera, &view) ||
      !ldk_camera_get_projection_matrix(editor->editor_camera,
          (float)width / height, &projection)) return false;
  Mat4 inv = mat4_inverse_full(mat4_mul(projection, view), &inverse_ok);
  if (!inverse_ok) return false;
  float ndc_x = 2.0f * (cursor.x - rect.x) / rect.w - 1.0f;
  float ndc_y = 1.0f - 2.0f * (cursor.y - rect.y) / rect.h;
  Mat4 world, inverse;
  if (!s_terrain_entity_world(editor, &world, &inverse)) return false;
  Vec3 near_point = mat4_mul_point(inverse,
      mat4_mul_point(inv, vec3_make(ndc_x, ndc_y, -1.0f)));
  Vec3 far_point = mat4_mul_point(inverse,
      mat4_mul_point(inv, vec3_make(ndc_x, ndc_y, 1.0f)));
  Vec3 ray = vec3_sub(far_point, near_point);
  float length = vec3_len(ray);
  if (length <= 0.00001f) return false;
  ray = vec3_div(ray, length);
  const LDKTerrain *terrain = &state->data;
  float ox = terrain->origin_x - (terrain->center_on_origin
      ? (float)terrain->width * terrain->cell_size * 0.5f : 0.0f);
  float oz = terrain->origin_z - (terrain->center_on_origin
      ? (float)terrain->height * terrain->cell_size * 0.5f : 0.0f);
  Vec3 bmin = vec3_make(ox, terrain->height_offset - 0.001f, oz);
  Vec3 bmax = vec3_make(ox + terrain->width * terrain->cell_size,
      terrain->height_offset + terrain->height_scale + 0.001f,
      oz + terrain->height * terrain->cell_size);
  float t0 = 0.0f, t1 = length;
  const float start[3] = {near_point.x, near_point.y, near_point.z};
  const float direction[3] = {ray.x, ray.y, ray.z};
  const float minimum[3] = {bmin.x, bmin.y, bmin.z};
  const float maximum[3] = {bmax.x, bmax.y, bmax.z};
  for (int axis = 0; axis < 3; ++axis)
  {
    if (fabsf(direction[axis]) < 1e-7f)
    {
      if (start[axis] < minimum[axis] || start[axis] > maximum[axis])
        return false;
      continue;
    }
    float a = (minimum[axis] - start[axis]) / direction[axis];
    float b = (maximum[axis] - start[axis]) / direction[axis];
    if (a > b) {float tmp = a; a = b; b = tmp;}
    if (a > t0) t0 = a;
    if (b < t1) t1 = b;
    if (t0 > t1) return false;
  }
  LDKImageInfo image = s_terrain_info(state);
  float step = fmaxf(terrain->cell_size * 0.25f, (t1-t0) / 2048.0f);
  float previous_t = t0;
  Vec3 previous = vec3_add(near_point, vec3_mul(ray, t0));
  float previous_height = 0.0f;
  bool previous_ok = ldk_terrain_surface_sample(terrain,
      &image, previous.x, previous.z, &previous_height, NULL);
  float previous_delta = previous.y - previous_height;
  for (float t = t0 + step; t <= t1 + step; t += step)
  {
    float current_t = fminf(t, t1);
    Vec3 point = vec3_add(near_point, vec3_mul(ray, current_t));
    float terrain_height = 0.0f;
    bool ok = ldk_terrain_surface_sample(terrain,
        &image, point.x, point.z, &terrain_height, NULL);
    if (ok && previous_ok && previous_delta >= 0.0f &&
        point.y <= terrain_height)
    {
      float lo = previous_t, hi = current_t;
      for (u32 j = 0; j < 12u; ++j)
      {
        float mid = (lo + hi) * 0.5f;
        Vec3 p = vec3_add(near_point, vec3_mul(ray, mid));
        float y;
        if (!ldk_terrain_surface_sample(terrain, &image,
                p.x, p.z, &y, NULL)) return false;
        if (p.y > y) lo = mid;
        else hi = mid;
      }
      *out = mat4_mul_point(world,
          vec3_add(near_point, vec3_mul(ray, (lo + hi) * 0.5f)));
      return true;
    }
    if (current_t >= t1) break;
    previous_t = current_t;
    previous_ok = ok;
    previous_delta = point.y - terrain_height;
  }
  return false;
}

void ldki_editor_terrain_view_update(
    LDKEditorContext *editor, LDKUIRect scene_view_rect)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  s_terrain_selection_sync(editor);
  state->hovered = false;
  LDKUIContext *ui = &editor->ui;
  if (!state->enabled || !state->loaded || !state->pixels ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED || !ui->mouse ||
      !ui->current_window ||
      ui->hovered_window_id != ui->current_window->id) return;

  /* Camera navigation takes precedence over sculpting. Ctrl+left drag and
   * the Pan tool both move the scene; middle/right drags pan/orbit it. */
  LDKKeyboardState keyboard;
  ldk_os_keyboard_state_get(&keyboard);
  if (editor->camera_controller.panning ||
      editor->camera_controller.orbiting ||
      editor->gizmo.mode == LDK_EDITOR_GIZMO_MODE_PAN ||
      ldk_os_keyboard_key_is_pressed(
          &keyboard, LDK_KEYCODE_LEFT_CONTROL))
  {
    if (state->stroke_active) s_terrain_stroke_finish(editor);
    return;
  }

  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
  if (!ldk_rectf_contains(&scene_view_rect, cursor.x, cursor.y) ||
      !ldk_rectf_contains(&ui->clip_rect, cursor.x, cursor.y) ||
      ui->active_id || ui->hot_id) return;
  Vec3 hit;
  if (!s_terrain_ray_hit(editor, scene_view_rect,
          ldk_pointf(cursor.x, cursor.y), &hit)) return;
  state->hovered = true;
  state->hover_position = hit;
  Mat4 world, inverse;
  if (!s_terrain_entity_world(editor, &world, &inverse)) return;
  Vec3 local_hit = mat4_mul_point(inverse, hit);
  if (!state->stroke_active &&
      ldk_os_mouse_button_down((LDKMouseState *)ui->mouse,
          LDK_MOUSE_BUTTON_LEFT))
    (void)s_terrain_stroke_begin(editor);
  if (state->stroke_active &&
      ldk_os_mouse_button_is_pressed((LDKMouseState *)ui->mouse,
          LDK_MOUSE_BUTTON_LEFT))
  {
    if (state->edit_mode == LDK_EDITOR_TERRAIN_EDIT_VEGETATION)
      (void)s_terrain_vegetation_brush_apply(editor, local_hit);
    else
      (void)s_terrain_brush_apply(editor, local_hit);
  }
}

void ldki_editor_terrain_frame_end(LDKEditorContext *editor)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  if (state->stroke_active && (!editor->ui.mouse ||
          !ldk_os_mouse_button_is_pressed(
              (LDKMouseState *)editor->ui.mouse, LDK_MOUSE_BUTTON_LEFT)))
    s_terrain_stroke_finish(editor);
}

void ldki_editor_terrain_submit(LDKEditorContext *editor)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  if (!state->enabled || !state->hovered || !state->pixels ||
      !editor->renderer) return;
  LDKDebugDraw draw = ldk_debug_draw_make(
      editor->renderer, editor->scene_view);
  draw.color = editor->debug_color;
  draw.thickness = 0.045f;
  draw.depth_test = true;
  Mat4 world, inverse;
  if (!s_terrain_entity_world(editor, &world, &inverse)) return;
  Vec3 local_hit = mat4_mul_point(inverse, state->hover_position);
  LDKImageInfo info = s_terrain_info(state);
  float rings[2] = {state->radius,
      state->radius * (1.0f - state->falloff)};
  for (u32 ring = 0u; ring < 2u; ++ring)
  {
    float radius = rings[ring];
    if (radius < 0.01f) continue;
    Vec3 previous = {0};
    bool previous_ok = false;
    for (u32 i = 0u; i <= TERRAIN_BRUSH_SEGMENTS; ++i)
    {
      float a = TERRAIN_PI * 2.0f * (float)i /
          (float)TERRAIN_BRUSH_SEGMENTS;
      Vec3 point = vec3_make(
          local_hit.x + cosf(a) * radius, 0.0f,
          local_hit.z + sinf(a) * radius);
      bool ok = ldk_terrain_surface_sample(&state->data, &info,
          point.x, point.z, &point.y, NULL);
      if (ok)
      {
        point.y += 0.08f;
        point = mat4_mul_point(world, point);
      }
      if (ok && previous_ok) (void)ldk_debug_draw_line(
          &draw, previous, point);
      previous = point;
      previous_ok = ok;
    }
  }
}

void ldki_editor_terrain_terminate(LDKEditorContext *editor)
{
  if (editor) s_terrain_reset(editor, false);
}

bool ldki_editor_terrain_project_close(LDKEditorContext *editor)
{
  s_terrain_reset(editor, false);
  memset(&s_terrain_vegetation_folds, 0,
      sizeof(s_terrain_vegetation_folds));
  memset(&s_terrain_surface_folds, 0,
      sizeof(s_terrain_surface_folds));
  return true;
}

/* The editor's property-row widgets provide the same draggable label/control
 * divider used by the Inspector. Keep the input parsing and commit semantics
 * here: target grid fields accept intermediate text while the user types. */
static u32 s_terrain_uint_row_input(LDKUIContext *ui, void *user,
    u32 index, char *buffer, u32 capacity, const float *float_value)
{
  (void)user;
  (void)index;
  (void)float_value;
  return ldk_ui_input_box_typed(ui, buffer, capacity,
      LDK_UI_INPUT_TYPE_UNSIGNED_INTEGER);
}

static u32 s_terrain_float_row_input(LDKUIContext *ui, void *user,
    u32 index, char *buffer, u32 capacity, const float *float_value)
{
  (void)user;
  (void)index;
  (void)float_value;
  return ldk_ui_input_box_typed(ui, buffer, capacity,
      LDK_UI_INPUT_TYPE_DECIMAL);
}

static void s_terrain_grid_input(LDKEditorContext *editor,
    const char *label, char *buffer, u32 capacity, u32 *value)
{
  char *end;
  unsigned long parsed;
  LDKUIContext *ui = &editor->ui;
  ldk_ui_push_id_cstr(ui, label);
  u32 result = ldk_editor_ui_row_uint(editor, label, 0u, buffer,
      capacity, false, s_terrain_uint_row_input, NULL, NULL);
  ldk_ui_end_horizontal(ui);
  if (result & (LDK_UI_INPUT_BOX_CHANGED | LDK_UI_INPUT_BOX_COMMITTED))
  {
    parsed = strtoul(buffer, &end, 10);
    if (end != buffer && *end == '\0' && parsed >= 1ul &&
        parsed <= 4096ul) *value = (u32)parsed;
  }
  if (result & (LDK_UI_INPUT_BOX_COMMITTED | LDK_UI_INPUT_BOX_CANCELED))
    snprintf(buffer, capacity, "%u", *value);
  ldk_ui_pop_id(ui);
}

static bool s_terrain_real_input(LDKEditorContext *editor,
    const char *label, char *buffer, u32 capacity,
    float *value, float minimum, float maximum)
{
  char *end;
  float parsed;
  LDKUIContext *ui = &editor->ui;
  ldk_ui_push_id_cstr(ui, label);
  u32 result = ldk_editor_ui_row_float(editor, label, 0u, buffer,
      capacity, value, false, s_terrain_float_row_input, NULL, NULL);
  ldk_ui_end_horizontal(ui);
  bool changed = false;
  /* Accept valid values as they are typed. Otherwise clicking Import or
   * Save before pressing Enter uses the previous Height Range. Keep the
   * text intact until commit/cancel so intermediate input is editable. */
  if (result & (LDK_UI_INPUT_BOX_CHANGED | LDK_UI_INPUT_BOX_COMMITTED))
  {
    parsed = strtof(buffer, &end);
    if (end != buffer && *end == '\0' && isfinite(parsed) &&
        parsed >= minimum && parsed <= maximum && parsed != *value)
    {
      *value = parsed;
      changed = true;
    }
  }
  if (result & (LDK_UI_INPUT_BOX_COMMITTED | LDK_UI_INPUT_BOX_CANCELED))
    snprintf(buffer, capacity, "%.6g", (double)*value);
  ldk_ui_pop_id(ui);
  return changed;
}

/* Surface changes are validated as a whole, so a partially edited array
 * never reaches the scene. The sculpt undo captures positional height rules;
 * changes to those rules invalidate older sculpt snapshots. */
static bool s_terrain_surface_apply(LDKEditorContext *editor,
    const LDKTerrain *candidate, bool height_rules_changed)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  if (!state->loaded || !ldk_terrain_validate(candidate)) return false;
  if (state->stroke_active) s_terrain_stroke_finish(editor);
  LDKTerrain previous = state->data;
  state->data = *candidate;
  if (!s_terrain_publish(editor))
  {
    state->data = previous;
    return false;
  }
  if (height_rules_changed) s_terrain_undo_discard(editor);
  state->dirty = true;
  return true;
}

static void s_terrain_surface_folds_sync(LDKEntity entity)
{
  LDKEditorTerrainSurfaceFolds *folds = &s_terrain_surface_folds;
  if (!folds->initialized ||
      !ldki_editor_entity_equal(folds->owner, entity))
  {
    memset(folds, 0, sizeof(*folds));
    folds->owner = entity;
    folds->initialized = true;
    folds->expanded = true;
    folds->surfaces[0].expanded = true;
  }
}

/* The ordered height bands belong to the *slots*. When two elevation
 * surfaces trade places, keep the thresholds and blends at their old slots:
 * the user is exchanging appearances, not silently making height rules
 * non-monotonic. Slope overlays retain their own settings and may move only
 * if the resulting terrain passes its normal validation. */
static bool s_terrain_surface_swapped(const LDKTerrain *source,
    u32 first, u32 second, LDKTerrain *result)
{
  if (!source || !result || first >= source->surface_count ||
      second >= source->surface_count || first == second) return false;
  *result = *source;
  LDKTerrainSurface temporary = result->surfaces[first];
  result->surfaces[first] = result->surfaces[second];
  result->surfaces[second] = temporary;
  if (!source->surfaces[first].use_slope &&
      !source->surfaces[second].use_slope)
  {
    result->surfaces[first].min_height = source->surfaces[first].min_height;
    result->surfaces[first].blend_width = source->surfaces[first].blend_width;
    result->surfaces[second].min_height = source->surfaces[second].min_height;
    result->surfaces[second].blend_width = source->surfaces[second].blend_width;
  }
  return ldk_terrain_validate(result);
}

static bool s_terrain_surface_move(LDKEditorContext *editor,
    u32 first, u32 second)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  LDKTerrain candidate;
  if (!s_terrain_surface_swapped(&state->data, first, second, &candidate) ||
      !s_terrain_surface_apply(editor, &candidate, true)) return false;
  LDKEditorTerrainSurfaceFold temporary =
      s_terrain_surface_folds.surfaces[first];
  s_terrain_surface_folds.surfaces[first] =
      s_terrain_surface_folds.surfaces[second];
  s_terrain_surface_folds.surfaces[second] = temporary;
  return true;
}

static bool s_terrain_surface_removed(const LDKTerrain *source,
    u32 index, LDKTerrain *result)
{
  if (!source || !result || source->surface_count <= 1u ||
      index >= source->surface_count) return false;
  *result = *source;
  for (u32 j = index; j + 1u < result->surface_count; ++j)
    result->surfaces[j] = result->surfaces[j + 1u];
  --result->surface_count;
  memset(&result->surfaces[result->surface_count], 0,
      sizeof(result->surfaces[0]));
  return ldk_terrain_validate(result);
}

static bool s_terrain_surface_remove(LDKEditorContext *editor, u32 index)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  LDKTerrain candidate;
  if (!s_terrain_surface_removed(&state->data, index, &candidate) ||
      !s_terrain_surface_apply(editor, &candidate, true)) return false;
  for (u32 i = index; i + 1u < LDK_TERRAIN_SURFACE_COUNT; ++i)
    s_terrain_surface_folds.surfaces[i] =
        s_terrain_surface_folds.surfaces[i + 1u];
  memset(&s_terrain_surface_folds.surfaces[LDK_TERRAIN_SURFACE_COUNT - 1u],
      0, sizeof(s_terrain_surface_folds.surfaces[0]));
  return true;
}

static bool s_terrain_surface_add(LDKEditorContext *editor)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  if (state->data.surface_count >= LDK_TERRAIN_SURFACE_COUNT) return false;
  LDKTerrain candidate = state->data;
  const u32 index = candidate.surface_count;
  candidate.surfaces[index] = candidate.surfaces[index - 1u];
  LDKTerrainSurface *surface = &candidate.surfaces[index];
  /* Reuse the familiar default naming pattern without duplicating names
   * after a previous deletion or a manual rename. */
  for (u32 suffix = index;; ++suffix)
  {
    snprintf(surface->name, sizeof(surface->name), "Surface %u", suffix);
    bool name_used = false;
    for (u32 j = 0u; j < index; ++j)
      if (strcmp(surface->name, candidate.surfaces[j].name) == 0)
      {
        name_used = true;
        break;
      }
    if (!name_used) break;
  }
  /* Manual paint overrides are per surface. Do not inherit a previous
   * surface's mask path when adding a new one. */
  memset(&surface->paint_mask_path, 0, sizeof(surface->paint_mask_path));
  /* Append a height surface above all existing height thresholds, even when
   * the last existing surface happens to be a slope overlay. */
  float highest = 0.0f;
  for (u32 i = 0u; i < index; ++i)
    if (!candidate.surfaces[i].use_slope &&
        candidate.surfaces[i].min_height > highest)
      highest = candidate.surfaces[i].min_height;
  surface->use_slope = false;
  surface->min_height = highest + (1.0f - highest) * 0.5f;
  ++candidate.surface_count;
  if (!s_terrain_surface_apply(editor, &candidate, true)) return false;
  memset(&s_terrain_surface_folds.surfaces[index], 0,
      sizeof(s_terrain_surface_folds.surfaces[index]));
  s_terrain_surface_folds.surfaces[index].expanded = true;
  return true;
}

static void s_terrain_surface_name_draw(LDKEditorContext *editor,
    u32 index, LDKEditorTerrainSurfaceFold *fold)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  const LDKTerrainSurface *surface = &state->data.surfaces[index];
  if (!fold->editing_name)
    snprintf(fold->name_text, sizeof(fold->name_text), "%s", surface->name);
  const u32 result = ldk_editor_ui_row_text(editor, "Name", 1u,
      fold->name_text, sizeof(fold->name_text), false, NULL);
  ldk_ui_end_horizontal(&editor->ui);
  if (result & LDK_UI_INPUT_BOX_CHANGED) fold->editing_name = true;
  if (result & LDK_UI_INPUT_BOX_CANCELED) fold->editing_name = false;
  if (result & LDK_UI_INPUT_BOX_COMMITTED)
  {
    fold->editing_name = false;
    if (fold->name_text[0] && strcmp(fold->name_text, surface->name))
    {
      LDKTerrain candidate = state->data;
      snprintf(candidate.surfaces[index].name,
          sizeof(candidate.surfaces[index].name), "%s", fold->name_text);
      (void)s_terrain_surface_apply(editor, &candidate, false);
    }
  }
}

static void s_terrain_surface_sprite_draw(LDKEditorContext *editor,
    u32 index, LDKEditorTerrainSurfaceFold *fold)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  const u64 sprite = state->data.surfaces[index].sprite_id;
  if (!fold->editing_sprite)
    snprintf(fold->sprite_text, sizeof(fold->sprite_text), "%llu",
        (unsigned long long)sprite);
  const u32 result = ldk_editor_ui_row_text(editor, "Atlas Sprite ID", 1u,
      fold->sprite_text, sizeof(fold->sprite_text), false, NULL);
  ldk_ui_end_horizontal(&editor->ui);
  if (result & LDK_UI_INPUT_BOX_CHANGED) fold->editing_sprite = true;
  if (result & LDK_UI_INPUT_BOX_CANCELED) fold->editing_sprite = false;
  if (result & LDK_UI_INPUT_BOX_COMMITTED)
  {
    fold->editing_sprite = false;
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = strtoull(fold->sprite_text, &end, 10);
    if (fold->sprite_text[0] && *fold->sprite_text != '-' &&
        end && *end == '\0' && errno != ERANGE && parsed <= INT64_MAX &&
        (u64)parsed != sprite)
    {
      LDKTerrain candidate = state->data;
      candidate.surfaces[index].sprite_id = (u64)parsed;
      (void)s_terrain_surface_apply(editor, &candidate, false);
    }
  }
}

static bool s_terrain_surface_float_row(LDKEditorContext *editor,
    const char *label, float *value, float minimum, float maximum)
{
  const float next = ldk_editor_ui_row_slider_float(editor, label, 1u,
      *value, minimum, maximum, false, NULL);
  ldk_ui_end_horizontal(&editor->ui);
  if (next == *value) return false;
  *value = next;
  return true;
}

static void s_terrain_surface_properties_draw(LDKEditorContext *editor,
    u32 index, LDKEditorTerrainSurfaceFold *fold)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  s_terrain_surface_name_draw(editor, index, fold);
  /* Recreate after the name is committed, so edits never use stale data. */
  LDKTerrain candidate = state->data;
  LDKTerrainSurface *surface = &candidate.surfaces[index];
  bool changed = false;
  bool height_rules_changed = false;
  if (index > 0u)
  {
    const bool slope = ldk_editor_ui_row_bool(editor, "Use Slope", 1u,
        surface->use_slope, false, NULL);
    ldk_ui_end_horizontal(&editor->ui);
    if (slope != surface->use_slope)
    {
      surface->use_slope = slope;
      if (!slope)
      {
        /* Returning from an overlay to a height band must respect the
         * neighboring bands, not introduce an invalid ordering. */
        float lower = 0.0f, upper = 1.0f;
        for (u32 j = 0u; j < index; ++j)
          if (!candidate.surfaces[j].use_slope)
            lower = candidate.surfaces[j].min_height;
        for (u32 j = index + 1u; j < candidate.surface_count; ++j)
          if (!candidate.surfaces[j].use_slope)
          {
            upper = candidate.surfaces[j].min_height;
            break;
          }
        surface->min_height = fminf(upper,
            fmaxf(lower, surface->min_height));
      }
      changed = true;
      height_rules_changed = true;
    }
  }
  if (index == 0u)
    ldk_editor_ui_row_label(editor, "Height Rule", 1u,
        "Base surface (always active)");
  if (surface->use_slope)
  {
    changed |= s_terrain_surface_float_row(editor, "Min Slope (degrees)",
        &surface->min_slope_degrees, 0.0f, 90.0f);
    changed |= s_terrain_surface_float_row(editor, "Slope Blend (degrees)",
        &surface->slope_blend_degrees, 0.0f, 90.0f);
  }
  else if (index > 0u)
  {
    float lower = 0.0f, upper = 1.0f;
    for (u32 j = 0u; j < index; ++j)
      if (!candidate.surfaces[j].use_slope)
        lower = candidate.surfaces[j].min_height;
    for (u32 j = index + 1u; j < candidate.surface_count; ++j)
      if (!candidate.surfaces[j].use_slope)
      {
        upper = candidate.surfaces[j].min_height;
        break;
      }
    const bool height_changed = s_terrain_surface_float_row(editor,
        "Min Height", &surface->min_height, lower, upper);
    const bool blend_changed = s_terrain_surface_float_row(editor,
        "Height Blend", &surface->blend_width, 0.0f, 1.0f);
    height_rules_changed |= height_changed || blend_changed;
    changed |= height_changed || blend_changed;
  }
  changed |= ldk_editor_ui_row_color(editor, "Color", 1u,
      &surface->color, false, NULL);
  changed |= ldk_editor_ui_row_color(editor, "Noise Low Color", 1u,
      &surface->noise_low_color, false, NULL);
  changed |= ldk_editor_ui_row_color(editor, "Noise High Color", 1u,
      &surface->noise_high_color, false, NULL);
  changed |= s_terrain_surface_float_row(editor, "Texture Weight",
      &surface->texture_weight, 0.0f, 1.0f);
  changed |= s_terrain_surface_float_row(editor, "UV Scale X",
      &surface->uv_scale.x, -64.0f, 64.0f);
  changed |= s_terrain_surface_float_row(editor, "UV Scale Y",
      &surface->uv_scale.y, -64.0f, 64.0f);
  if (changed)
    (void)s_terrain_surface_apply(editor, &candidate, height_rules_changed);
  s_terrain_surface_sprite_draw(editor, index, fold);
}

static void s_terrain_surface_folds_draw(LDKEditorContext *editor,
    LDKEntity entity)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  LDKUIContext *ui = &editor->ui;
  s_terrain_surface_folds_sync(entity);
  ldk_ui_push_id_cstr(ui, "terrain-surfaces");
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_weight(ui, 1.0f);
  const u32 header = ldk_ui_tree_node_ex(ui, "Surfaces",
      (LDKUIIcon){0}, s_terrain_surface_folds.expanded, 0u,
      LDK_UI_TREE_NODE_NONE);
  if (header & LDK_UI_TREE_NODE_RESULT_TOGGLED)
    s_terrain_surface_folds.expanded =
        !s_terrain_surface_folds.expanded;
  ldk_ui_set_next_width(ui, ldk_ui_px(28.0f));
  ldk_ui_set_next_disabled(ui, state->stroke_active ||
      state->data.surface_count >= LDK_TERRAIN_SURFACE_COUNT);
  const bool add = ldk_ui_icon_button(ui,
      s_terrain_ui_icon(editor, LDK_EDITOR_ICON_PLUS_BUTTON), "");
  ldk_ui_end_horizontal(ui);
  if (add && !s_terrain_surface_add(editor))
    ldki_editor_log_warning(editor, "Could not add terrain surface.");
  if (s_terrain_surface_folds.expanded)
  {
    for (u32 i = 0u; i < state->data.surface_count; ++i)
    {
      LDKEditorTerrainSurfaceFold *fold =
          &s_terrain_surface_folds.surfaces[i];
      LDKTerrain candidate;
      const bool up = i > 0u &&
          s_terrain_surface_swapped(&state->data, i - 1u, i, &candidate);
      const bool down = i + 1u < state->data.surface_count &&
          s_terrain_surface_swapped(&state->data, i, i + 1u, &candidate);
      const bool can_delete = s_terrain_surface_removed(&state->data,
          i, &candidate);
      ldk_ui_push_id_u32(ui, i);
      ldk_ui_begin_horizontal(ui);
      ldk_ui_set_next_weight(ui, 1.0f);
      const u32 result = ldk_ui_tree_node_ex(ui,
          state->data.surfaces[i].name, (LDKUIIcon){0},
          fold->expanded, 1u, LDK_UI_TREE_NODE_NONE);
      if (result & (LDK_UI_TREE_NODE_RESULT_TOGGLED |
          LDK_UI_TREE_NODE_RESULT_CLICKED))
        fold->expanded = !fold->expanded;
      ldk_ui_set_next_width(ui, ldk_ui_px(24.0f));
      ldk_ui_set_next_disabled(ui, !up || state->stroke_active);
      const bool move_up = ldk_ui_icon_button(ui,
          s_terrain_ui_icon(editor, LDK_EDITOR_ICON_UP_ARROW), "");
      ldk_ui_set_next_width(ui, ldk_ui_px(24.0f));
      ldk_ui_set_next_disabled(ui, !down || state->stroke_active);
      const bool move_down = ldk_ui_icon_button(ui,
          s_terrain_ui_icon(editor, LDK_EDITOR_ICON_DOWN_ARROW), "");
      ldk_ui_set_next_width(ui, ldk_ui_px(24.0f));
      ldk_ui_set_next_disabled(ui, !can_delete || state->stroke_active);
      const bool remove = ldk_ui_icon_button(ui,
          s_terrain_ui_icon(editor, LDK_EDITOR_ICON_MINUS_BUTTON), "");
      ldk_ui_end_horizontal(ui);
      if (move_up || move_down || remove)
      {
        if (move_up) (void)s_terrain_surface_move(editor, i - 1u, i);
        else if (move_down) (void)s_terrain_surface_move(editor, i, i + 1u);
        else (void)s_terrain_surface_remove(editor, i);
        ldk_ui_pop_id(ui);
        break; /* The element ordering and widget IDs have changed. */
      }
      if (fold->expanded)
        s_terrain_surface_properties_draw(editor, i, fold);
      ldk_ui_pop_id(ui);
    }
  }
  ldk_ui_pop_id(ui);
}

static void s_terrain_vegetation_folds_sync(LDKEntity entity,
    const LDKEditorTerrainState *state)
{
  LDKEditorTerrainVegetationFolds *folds = &s_terrain_vegetation_folds;
  if (!folds->initialized ||
      !ldki_editor_entity_equal(folds->owner, entity))
  {
    memset(folds, 0, sizeof(*folds));
    folds->owner = entity;
    folds->initialized = true;
    folds->expanded = true;
  }

  /* Removing and adding layers must not exhaust the fixed-size fold cache. */
  for (u32 i = 0u; i < folds->count;)
  {
    bool exists = false;
    for (u32 j = 0u; j < state->data.vegetation_layer_count; ++j)
    {
      if (state->data.vegetation_layers[j].id == folds->layers[i].layer_id)
      {
        exists = true;
        break;
      }
    }
    if (exists)
    {
      ++i;
      continue;
    }
    if (i + 1u < folds->count)
      memmove(&folds->layers[i], &folds->layers[i + 1u],
          (size_t)(folds->count - i - 1u) * sizeof(folds->layers[0]));
    --folds->count;
  }
}

static LDKEditorTerrainVegetationFold *s_terrain_vegetation_fold_get(
    const LDKTerrainVegetationLayer *layer, bool selected)
{
  LDKEditorTerrainVegetationFolds *folds = &s_terrain_vegetation_folds;
  for (u32 i = 0u; i < folds->count; ++i)
    if (folds->layers[i].layer_id == layer->id)
      return &folds->layers[i];
  if (folds->count >= LDK_TERRAIN_VEGETATION_LAYER_COUNT) return NULL;
  LDKEditorTerrainVegetationFold *fold = &folds->layers[folds->count++];
  memset(fold, 0, sizeof(*fold));
  fold->layer_id = layer->id;
  fold->expanded = selected;
  snprintf(fold->name_text, sizeof(fold->name_text), "%s",
      x_smallstr_cstr(&layer->type.name));
  return fold;
}

/* The name is a label, never the painter's lookup key. Commit only a complete
 * valid name, keeping intermediate edits in stable editor-owned memory. */
static void s_terrain_vegetation_name_draw(LDKEditorContext *editor,
    const LDKTerrainVegetationLayer *layer,
    LDKEditorTerrainVegetationFold *fold)
{
  LDKUIContext *ui = &editor->ui;
  if (!fold->editing_name)
    snprintf(fold->name_text, sizeof(fold->name_text), "%s",
        x_smallstr_cstr(&layer->type.name));

  const u32 result = ldk_editor_ui_row_text(editor, "Name", 1u,
      fold->name_text, sizeof(fold->name_text), false, NULL);
  ldk_ui_end_horizontal(ui);
  if (result & LDK_UI_INPUT_BOX_CHANGED) fold->editing_name = true;
  if (result & LDK_UI_INPUT_BOX_CANCELED)
    fold->editing_name = false;
  if (result & LDK_UI_INPUT_BOX_COMMITTED)
  {
    fold->editing_name = false;
    if (fold->name_text[0] &&
        strcmp(fold->name_text, x_smallstr_cstr(&layer->type.name)) != 0)
    {
      LDKEditorTerrainState *state = &editor->terrain_editor;
      const u32 index = (u32)(layer - state->data.vegetation_layers);
      if (index < state->data.vegetation_layer_count)
      {
        LDKTerrain candidate = state->data;
        if (x_smallstr_init(&candidate.vegetation_layers[index].type.name,
                fold->name_text) >= 0 && ldk_terrain_validate(&candidate))
        {
          if (state->stroke_active) s_terrain_stroke_finish(editor);
          LDKTerrain previous = state->data;
          state->data = candidate;
          if (!s_terrain_publish(editor)) state->data = previous;
        }
      }
    }
  }
}

/* Deleting a layer never reassigns another layer's density mask to the brush.
 * Only deletion of the active layer changes the painter's selection. */
static bool s_terrain_vegetation_layer_delete(LDKEditorContext *editor,
    u64 layer_id, u32 index)
{
  LDKEditorTerrainState *state = &editor->terrain_editor;
  if (state->stroke_active) s_terrain_stroke_finish(editor);
  LDKTerrainComponent *component = s_terrain_owned_component(state);
  if (!component ||
      !ldk_terrain_component_vegetation_layer_remove(component, layer_id))
    return false;

  s_terrain_undo_discard(editor);
  state->data = component->terrain;
  state->observed_revision = component->revision;
  if (state->vegetation_layer_id == layer_id)
  {
    free(state->vegetation_pixels);
    state->vegetation_pixels = NULL;
    state->vegetation_layer_id = 0u;
    if (state->data.vegetation_layer_count)
    {
      u32 next = index < state->data.vegetation_layer_count ? index :
          state->data.vegetation_layer_count - 1u;
      (void)s_terrain_vegetation_select(editor,
          state->data.vegetation_layers[next].id);
    }
  }
  ldki_editor_scene_dirty_set(editor, true);
  return true;
}

/* Every terrain layer owns the entire vegetation type description. */
static LDKGrassTypeDesc s_terrain_vegetation_type_make(const char *name)
{
  LDKGrassTypeDesc desc;
  ldk_grass_type_desc_defaults(&desc);
  (void)x_smallstr_init(&desc.name, name);
  return desc;
}

static bool s_terrain_vegetation_float_row(LDKEditorContext *editor,
    const char *label, float *value, float minimum, float maximum)
{
  float next = ldk_editor_ui_row_slider_float(editor, label, 1u,
      *value, minimum, maximum, false, NULL);
  ldk_ui_end_horizontal(&editor->ui);
  if (next == *value) return false;
  *value = next;
  return true;
}

static bool s_terrain_vegetation_bool_row(LDKEditorContext *editor,
    const char *label, bool *value)
{
  bool next = ldk_editor_ui_row_bool(editor, label, 1u,
      *value, false, NULL);
  ldk_ui_end_horizontal(&editor->ui);
  if (next == *value) return false;
  *value = next;
  return true;
}

/* Built-in type inspector, using the same rows as other component properties.
 * The per-layer descriptor belongs to TerrainComponent; GrassSystem keeps
 * wind/LOD/material settings and the runtime patch implementation. */
static void s_terrain_vegetation_type_inspector(LDKEditorContext *editor,
    LDKTerrainVegetationLayer *layer)
{
  if (!editor || !layer) return;
  LDKUIContext *ui = &editor->ui;
  LDKGrassTypeDesc type = layer->type;
  bool changed = false;
  static const char *const presets[] = {
      "Thin", "Crossed", "Tuft", "Flat Top", "Thorny", "Plumed",
      "Custom 0", "Custom 1", "Custom 2", "Custom 3",
      "Custom 4", "Custom 5", "Custom 6", "Custom 7"};
  ldk_ui_push_id_cstr(ui, "terrain-vegetation-type");
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, "Vegetation Type");
  u32 preset = ldk_editor_ui_row_enum(editor, "Blade Preset", 1u,
      presets, (u32)(sizeof(presets) / sizeof(presets[0])),
      (u32)type.blade_preset, false, NULL);
  if (preset != (u32)type.blade_preset)
  {
    type.blade_preset = (LDKGrassBladePreset)preset;
    changed = true;
  }
  changed |= s_terrain_vegetation_float_row(editor, "Blade Density",
      &type.density, 0.0f, fmaxf(10.0f, type.density));
  changed |= s_terrain_vegetation_float_row(editor, "Clumping",
      &type.clumping, 0.0f, 1.0f);
  changed |= s_terrain_vegetation_float_row(editor, "Clump Radius",
      &type.clump_radius, 0.0f, fmaxf(10.0f, type.clump_radius));
  changed |= s_terrain_vegetation_float_row(editor, "Min Blade Height",
      &type.min_height, 0.01f, fmaxf(20.0f, type.min_height));
  changed |= s_terrain_vegetation_float_row(editor, "Max Blade Height",
      &type.max_height, type.min_height, fmaxf(20.0f, type.max_height));
  changed |= s_terrain_vegetation_float_row(editor, "Blade Width",
      &type.blade_width, 0.01f, fmaxf(5.0f, type.blade_width));
  changed |= s_terrain_vegetation_float_row(editor, "Curvature",
      &type.curvature, 0.0f, fmaxf(1.0f, type.curvature));
  changed |= s_terrain_vegetation_float_row(editor, "Wind Strength",
      &type.wind_strength, 0.0f, fmaxf(1.0f, type.wind_strength));
  changed |= s_terrain_vegetation_float_row(editor, "Recovery Time",
      &type.interaction_recovery_time, 0.0f, 16.0f);
  changed |= s_terrain_vegetation_bool_row(editor, "Noise Height",
      &type.use_noise_height);
  changed |= ldk_editor_ui_row_color(editor, "Bottom Color", 1u,
      &type.bottom_color, false, NULL);
  changed |= ldk_editor_ui_row_color(editor, "Top Color", 1u,
      &type.top_color, false, NULL);
  changed |= s_terrain_vegetation_bool_row(editor, "Noise Color",
      &type.use_noise_color);
  changed |= s_terrain_vegetation_bool_row(editor, "Custom Noise Palette",
      &type.use_custom_noise_palette);
  if (type.use_custom_noise_palette)
  {
    changed |= ldk_editor_ui_row_color(editor, "Noise Color 0", 1u,
        &type.noise_color_stop_0_color, false, NULL);
    changed |= ldk_editor_ui_row_color(editor, "Noise Color 1", 1u,
        &type.noise_color_stop_1_color, false, NULL);
    changed |= ldk_editor_ui_row_color(editor, "Noise Color 2", 1u,
        &type.noise_color_stop_2_color, false, NULL);
    changed |= ldk_editor_ui_row_color(editor, "Noise Color 3", 1u,
        &type.noise_color_stop_3_color, false, NULL);
  }
  if (changed)
  {
    LDKTerrain candidate = editor->terrain_editor.data;
    u32 index = (u32)(layer - editor->terrain_editor.data.vegetation_layers);
    if (index < candidate.vegetation_layer_count)
    {
      candidate.vegetation_layers[index].type = type;
      if (ldk_terrain_validate(&candidate))
      {
        LDKTerrain previous = editor->terrain_editor.data;
        editor->terrain_editor.data = candidate;
        if (!s_terrain_publish(editor))
          editor->terrain_editor.data = previous;
      }
    }
  }
  ldk_ui_pop_id(ui);
}

void ldki_editor_terrain_inspector_draw(LDKEditorContext *editor,
    LDKEntity entity)
{
  /* The component Inspector already resolved the selected entity. Avoid
   * silently skipping the built-in editor on a redundant selection check. */
  if (!editor || x_handle_is_null(entity) ||
      !ldk_ecs_component_get_const(entity, LDK_COMPONENT_TYPE_TERRAIN))
    return;
  LDKEditorTerrainState *state = &editor->terrain_editor;
  LDKUIContext *ui = &editor->ui;
  s_terrain_selection_sync(editor);
  ldk_editor_ui_row_column_update(editor);
  if (!state->target_width) state->target_width = 256u;
  if (!state->target_height) state->target_height = 256u;
  if (state->target_cell_size <= 0.0f) state->target_cell_size = 1.0f;
  if (state->height_range <= 0.0f)
  {
    state->height_range = 64.0f;
    s_terrain_fields_sync(state);
  }
  ldk_ui_push_id_cstr(ui, "terrain-editor");
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, "Terrain Editor");
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, "Target terrain grid (Import)");
  s_terrain_grid_input(editor, "Width", state->width_text,
      sizeof(state->width_text), &state->target_width);
  s_terrain_grid_input(editor, "Depth", state->depth_text,
      sizeof(state->depth_text), &state->target_height);
  (void)s_terrain_real_input(editor, "Cell Size",
      state->cell_size_text, sizeof(state->cell_size_text),
      &state->target_cell_size, 0.001f, 1000.0f);
  char grid_info[128];
  snprintf(grid_info, sizeof(grid_info),
      "%.1f x %.1f world units",
      (double)(state->target_width * state->target_cell_size),
      (double)(state->target_height * state->target_cell_size));
  ldk_editor_ui_row_label(editor, "World Size", 0u, grid_info);
  /* Change the world-space maximum without changing the minimum. An
   * imported image keeps its normalized samples; this simply rescales the
   * vertical range and immediately updates the scene's terrain component. */
  const float min_height = state->data.height_offset;
  float max_height = min_height + state->height_range;
  if (s_terrain_real_input(editor, "Max Height",
          state->height_range_text, sizeof(state->height_range_text),
          &max_height, min_height + 0.001f, min_height + 100000.0f) &&
      !state->stroke_active && state->loaded)
  {
    LDKTerrain candidate = state->data;
    candidate.height_scale = max_height - min_height;
    if (ldk_terrain_validate(&candidate))
    {
      s_terrain_undo_discard(editor);
      state->data = candidate;
      state->height_range = candidate.height_scale;
      (void)s_terrain_publish(editor);
      state->dirty = true;
    }
  }
  if (!state->loaded ||
      !ldki_editor_entity_equal(state->owner_entity, entity))
  {
    ldk_editor_ui_row_label(editor, "Status", 0u,
        "Unable to load terrain component.");
    ldk_ui_pop_id(ui);
    return;
  }
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  if (ldk_ui_button(ui, "Create Flat Grid"))
  {
    if (!s_terrain_create_flat(editor))
      ldki_editor_log_warning(editor, "Failed to create a flat terrain grid.");
  }
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  if (ldk_ui_button(ui, "Import from Heightmap"))
  {
    if (!s_terrain_import(editor))
      ldki_editor_log_warning(editor, "Heightmap import cancelled or failed.");
  }
  ldk_ui_horizontal_line(ui);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, "Editing mode");
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  bool sculpt_active = state->enabled &&
      state->edit_mode == LDK_EDITOR_TERRAIN_EDIT_SCULPT;
  if (ldk_ui_toggle_icon_button(ui,
          s_terrain_ui_icon(editor, LDK_EDITOR_ICON_TERRAIN_EDITOR),
          "Sculpt", sculpt_active) != sculpt_active)
  {
    if (state->stroke_active) s_terrain_stroke_finish(editor);
    if (sculpt_active)
      state->enabled = false;
    else
    {
      state->enabled = true;
      state->edit_mode = LDK_EDITOR_TERRAIN_EDIT_SCULPT;
    }
  }
  ldk_ui_set_next_disabled(ui, true); /* Surface paint: separate stage. */
  (void)ldk_ui_button(ui, "Surface Paint");
  if (ldk_ui_button(ui,
          state->enabled && state->edit_mode ==
              LDK_EDITOR_TERRAIN_EDIT_VEGETATION
              ? "Exit Vegetation" : "Vegetation"))
  {
    if (state->stroke_active) s_terrain_stroke_finish(editor);
    if (state->enabled && state->edit_mode ==
        LDK_EDITOR_TERRAIN_EDIT_VEGETATION)
      state->enabled = false;
    else
    {
      state->enabled = true;
      state->edit_mode = LDK_EDITOR_TERRAIN_EDIT_VEGETATION;
    }
  }
  ldk_ui_end_horizontal(ui);
  if (state->edit_mode == LDK_EDITOR_TERRAIN_EDIT_SCULPT)
  {
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, "Sculpt");
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  bool raise_selected = state->sculpt_tool == LDK_EDITOR_TERRAIN_RAISE;
  if (ldk_ui_toggle_icon_button(ui,
          s_terrain_ui_icon(editor, LDK_EDITOR_ICON_TERRAIN_RAISE),
          "Raise", raise_selected) && !raise_selected)
    state->sculpt_tool = LDK_EDITOR_TERRAIN_RAISE;
  bool lower_selected = state->sculpt_tool == LDK_EDITOR_TERRAIN_LOWER;
  if (ldk_ui_toggle_icon_button(ui,
          s_terrain_ui_icon(editor, LDK_EDITOR_ICON_TERRAIN_LOWER),
          "Lower", lower_selected) && !lower_selected)
    state->sculpt_tool = LDK_EDITOR_TERRAIN_LOWER;
  if (ldk_ui_button(ui,
          state->sculpt_tool == LDK_EDITOR_TERRAIN_SMOOTH ?
              "[Smooth]" : "Smooth"))
    state->sculpt_tool = LDK_EDITOR_TERRAIN_SMOOTH;
  bool flatten_selected = state->sculpt_tool == LDK_EDITOR_TERRAIN_FLATTEN;
  if (ldk_ui_toggle_icon_button(ui,
          s_terrain_ui_icon(editor, LDK_EDITOR_ICON_TERRAIN_FLATTERN),
          "Flatten", flatten_selected) && !flatten_selected)
    state->sculpt_tool = LDK_EDITOR_TERRAIN_FLATTEN;
  ldk_ui_end_horizontal(ui);
  if (state->sculpt_tool == LDK_EDITOR_TERRAIN_FLATTEN)
    (void)s_terrain_real_input(editor, "Target Height",
        state->flatten_height_text, sizeof(state->flatten_height_text),
        &state->flatten_height, -100000.0f, 100000.0f);
  }
  else
  {
    ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
    ldk_ui_label(ui, "Vegetation Brush");
    ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
    ldk_ui_begin_horizontal(ui);
    if (ldk_ui_button(ui, state->vegetation_erase ? "Paint" : "[Paint]"))
    {
      if (state->stroke_active) s_terrain_stroke_finish(editor);
      state->vegetation_erase = false;
    }
    if (ldk_ui_button(ui, state->vegetation_erase ? "[Erase]" : "Erase"))
    {
      if (state->stroke_active) s_terrain_stroke_finish(editor);
      state->vegetation_erase = true;
    }
    ldk_ui_end_horizontal(ui);
    const char *active_name = NULL;
    for (u32 i = 0u; i < state->data.vegetation_layer_count; ++i)
    {
      const LDKTerrainVegetationLayer *layer =
          &state->data.vegetation_layers[i];
      if (layer->id == state->vegetation_layer_id)
      {
        active_name = x_smallstr_cstr(&layer->type.name);
        break;
      }
    }
    ldk_editor_ui_row_label(editor, "Brush Layer", 0u,
        active_name ? active_name : "Select a vegetation layer below.");
  }
  state->radius = ldk_editor_ui_row_slider_float(editor,
      "Brush Radius", 0u, state->radius, 0.5f, 128.0f, false, NULL);
  ldk_ui_end_horizontal(ui);
  state->strength = ldk_editor_ui_row_slider_float(editor,
      "Brush Strength", 0u, state->strength, 0.1f, 100.0f, false, NULL);
  ldk_ui_end_horizontal(ui);
  state->falloff = ldk_editor_ui_row_slider_float(editor,
      "Falloff", 0u, state->falloff, 0.0f, 1.0f, false, NULL);
  ldk_ui_end_horizontal(ui);
  ldk_ui_horizontal_line(ui);
  s_terrain_surface_folds_draw(editor, entity);
  s_terrain_vegetation_folds_sync(entity, state);
  LDKEditorTerrainVegetationFolds *folds = &s_terrain_vegetation_folds;
  ldk_ui_push_id_cstr(ui, "vegetation-fold");
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  u32 vegetation_header = ldk_ui_tree_node_ex(ui, "Vegetation",
      (LDKUIIcon){0}, folds->expanded, 0u, LDK_UI_TREE_NODE_NONE);
  if (vegetation_header & LDK_UI_TREE_NODE_RESULT_TOGGLED)
    folds->expanded = !folds->expanded;
  if (folds->expanded)
  {
    for (u32 i = 0u; i < state->data.vegetation_layer_count; ++i)
    {
      LDKTerrainVegetationLayer *layer = &state->data.vegetation_layers[i];
      const u64 layer_id = layer->id;
      const bool selected = layer_id == state->vegetation_layer_id;
      LDKEditorTerrainVegetationFold *fold =
          s_terrain_vegetation_fold_get(layer, selected);
      if (!fold) continue;

      /* IDs belong to the data, not the display name or array position. */
      ldk_ui_push_id_u32(ui, (u32)layer_id);
      ldk_ui_push_id_u32(ui, (u32)(layer_id >> 32u));
      ldk_ui_begin_horizontal(ui);
      ldk_ui_set_next_weight(ui, 1.0f);
      const u32 result = ldk_ui_tree_node_ex(ui,
          x_smallstr_cstr(&layer->type.name), (LDKUIIcon){0},
          fold->expanded, 1u,
          selected ? LDK_UI_TREE_NODE_SELECTED : LDK_UI_TREE_NODE_NONE);
      if (result & LDK_UI_TREE_NODE_RESULT_TOGGLED)
        fold->expanded = !fold->expanded;
      else if ((result & LDK_UI_TREE_NODE_RESULT_CLICKED) && !selected)
      {
        if (state->stroke_active) s_terrain_stroke_finish(editor);
        (void)s_terrain_vegetation_select(editor, layer_id);
      }

      LDKUIIcon delete_icon = s_terrain_ui_icon(editor,
          LDK_EDITOR_ICON_DELETE);
      ldk_ui_set_next_width(ui, ldk_ui_px(28.0f));
      bool remove = ldk_ui_icon_button(ui, delete_icon, "");
      ldk_ui_end_horizontal(ui);
      if (remove)
      {
        (void)s_terrain_vegetation_layer_delete(editor, layer_id, i);
        ldk_ui_pop_id(ui);
        ldk_ui_pop_id(ui);
        break;
      }

      if (fold->expanded)
      {
        s_terrain_vegetation_name_draw(editor, layer, fold);
        float density = ldk_editor_ui_row_slider_float(editor,
            "Density Scale", 1u, layer->density_scale, 0.0f, 5.0f,
            false, NULL);
        ldk_ui_end_horizontal(ui);
        if (density != layer->density_scale && !state->stroke_active)
        {
          LDKTerrain candidate = state->data;
          candidate.vegetation_layers[i].density_scale = density;
          if (ldk_terrain_validate(&candidate))
          {
            LDKTerrain previous = state->data;
            state->data = candidate;
            if (!s_terrain_publish(editor)) state->data = previous;
          }
        }
        s_terrain_vegetation_type_inspector(editor, layer);
      }
      ldk_ui_pop_id(ui);
      ldk_ui_pop_id(ui);
    }
  }
  if (folds->expanded)
  {
    (void)ldk_editor_ui_row_text(editor, "Layer Name (optional)",
        0u, state->vegetation_type_text,
        sizeof(state->vegetation_type_text), false, NULL);
    ldk_ui_end_horizontal(ui);
    ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
    if (ldk_ui_button(ui, "Add Vegetation Layer"))
    {
      LDKTerrain candidate = state->data;
      u64 layer_id = 0u;
      char default_name[LDK_TERRAIN_LAYER_NAME_CAPACITY];
      snprintf(default_name, sizeof(default_name), "Vegetation %llu",
          (unsigned long long)state->data.next_vegetation_layer_id);
      const char *name = state->vegetation_type_text[0]
          ? state->vegetation_type_text : default_name;
      LDKGrassTypeDesc type = s_terrain_vegetation_type_make(name);
      bool added = !state->stroke_active &&
          ldk_terrain_vegetation_layer_add(&candidate, &type, 1.0f,
              (u32)(candidate.next_vegetation_layer_id * 1664525u +
                  1013904223u), &layer_id);
      if (added)
      {
        LDKTerrain previous = state->data;
        state->data = candidate;
        if (s_terrain_publish(editor))
        {
          s_terrain_undo_discard(editor);
          (void)s_terrain_vegetation_select(editor, layer_id);
          LDKEditorTerrainVegetationFold *created =
              s_terrain_vegetation_fold_get(
                  &state->data.vegetation_layers[
                      state->data.vegetation_layer_count - 1u], true);
          if (created) created->expanded = true;
        }
        else
          state->data = previous;
      }
      else
        ldki_editor_log_warning(editor,
            "Could not add vegetation layer: invalid settings or layer limit.");
    }
  }
  ldk_ui_pop_id(ui);
  ldk_ui_horizontal_line(ui);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  if (ldk_ui_button(ui, "Export Heightmap..."))
  {
    if (!s_terrain_export_heightmap(editor))
      ldki_editor_log_warning(editor,
          "Heightmap PNG export cancelled or failed.");
  }
  /* Let only the final spacer absorb extra vertical space. All preceding
   * rows, sections and action buttons have their natural fixed height. */
  ldk_ui_spacer(ui);
  ldk_ui_pop_id(ui);
}
