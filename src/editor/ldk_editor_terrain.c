/* Built-in TerrainComponent inspector and scene-view sculpting. The scene
 * owns the editable height data; imported source images are never modified. */
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
  LDKEditorTerrainHeightSnapshot range_state;
  /* Region pixels follow, in row-major order. */
} LDKEditorTerrainUndoRegion;

static void s_terrain_stroke_finish(LDKEditorContext *editor);

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
  if (!state->stroke_start || !state->pixels) goto done;
  const u32 width = state->data.width, height = state->data.height;
  const u32 pixel_size = state->image_format ==
      LDK_IMAGE_FORMAT_RGBA16_UNORM ? 8u : 4u;
  u32 min_x = width, min_y = height, max_x = 0u, max_y = 0u;
  for (u32 y = 0u; y < height; ++y)
  {
    for (u32 x = 0u; x < width; ++x)
    {
      size_t at = ((size_t)y * width + x) * pixel_size;
      if (memcmp(state->pixels + at, state->stroke_start + at,
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
    if (memcmp(&after_height, &state->stroke_height_start,
            sizeof(after_height)) == 0) goto done;
    /* A metadata-only change still needs an Undo region header. */
    min_x = min_y = max_x = max_y = 0u;
  }
  const u32 rw = max_x - min_x + 1u, rh = max_y - min_y + 1u;
  const size_t pitch = (size_t)rw * pixel_size;
  const size_t bytes = sizeof(LDKEditorTerrainUndoRegion) + pitch * rh;
  u8 *before = malloc(bytes), *after = malloc(bytes);
  if (!before || !after) goto done_regions;
  LDKEditorTerrainUndoRegion header = {
      min_x, min_y, rw, rh, pixel_size, state->stroke_height_start};
  memcpy(before, &header, sizeof(header));
  header.range_state = after_height;
  memcpy(after, &header, sizeof(header));
  for (u32 y = 0u; y < rh; ++y)
  {
    size_t start = ((size_t)(min_y + y) * width + min_x) * pixel_size;
    memcpy(before + sizeof(header) + (size_t)y * pitch,
        state->stroke_start + start, pitch);
    memcpy(after + sizeof(header) + (size_t)y * pitch,
        state->pixels + start, pitch);
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
      region.pixel_size != 8u ||
      (u64)region.width * region.height * 8u > SIZE_MAX - sizeof(region) ||
      size != sizeof(region) + (size_t)region.width * region.height * 8u)
    return false;
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
    s_terrain_reset(editor, false);
    (void)s_terrain_bind(editor, editor->selected_entity, component);
    state->enabled = sculpt_enabled;
    state->sculpt_tool = tool;
    state->radius = radius;
    state->strength = strength;
    state->falloff = falloff;
    state->flatten_height = flatten_height;
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
  state->stroke_start = malloc(state->pixel_bytes);
  if (!state->stroke_start) return false;
  memcpy(state->stroke_start, state->pixels, state->pixel_bytes);
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
    (void)s_terrain_brush_apply(editor, local_hit);
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
  if (ldk_ui_button(ui, state->enabled ? "Exit Sculpt" : "Sculpt"))
    state->enabled = !state->enabled;
  ldk_ui_set_next_disabled(ui, true); /* Stage 6 */
  (void)ldk_ui_button(ui, "Surface Paint");
  ldk_ui_set_next_disabled(ui, true); /* Stage 5 */
  (void)ldk_ui_button(ui, "Vegetation");
  ldk_ui_end_horizontal(ui);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, "Sculpt");
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  if (ldk_ui_button(ui,
          state->sculpt_tool == LDK_EDITOR_TERRAIN_RAISE ?
              "[Raise]" : "Raise"))
    state->sculpt_tool = LDK_EDITOR_TERRAIN_RAISE;
  if (ldk_ui_button(ui,
          state->sculpt_tool == LDK_EDITOR_TERRAIN_LOWER ?
              "[Lower]" : "Lower"))
    state->sculpt_tool = LDK_EDITOR_TERRAIN_LOWER;
  if (ldk_ui_button(ui,
          state->sculpt_tool == LDK_EDITOR_TERRAIN_SMOOTH ?
              "[Smooth]" : "Smooth"))
    state->sculpt_tool = LDK_EDITOR_TERRAIN_SMOOTH;
  if (ldk_ui_button(ui,
          state->sculpt_tool == LDK_EDITOR_TERRAIN_FLATTEN ?
              "[Flatten]" : "Flatten"))
    state->sculpt_tool = LDK_EDITOR_TERRAIN_FLATTEN;
  ldk_ui_end_horizontal(ui);
  if (state->sculpt_tool == LDK_EDITOR_TERRAIN_FLATTEN)
    (void)s_terrain_real_input(editor, "Target Height",
        state->flatten_height_text, sizeof(state->flatten_height_text),
        &state->flatten_height, -100000.0f, 100000.0f);
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
  char label[128];
  snprintf(label, sizeof(label), "Surface Layers: %u",
      state->data.surface_count);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, label);
  for (u32 i = 0u; i < state->data.surface_count; ++i)
  {
    snprintf(label, sizeof(label), "Surface %u", i);
    ldk_editor_ui_row_label(editor, label, 0u,
        state->data.surfaces[i].name);
  }
  snprintf(label, sizeof(label), "Vegetation Layers: %u",
      state->data.vegetation_layer_count);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, label);
  for (u32 i = 0u; i < state->data.vegetation_layer_count; ++i)
  {
    const LDKTerrainVegetationLayer *layer =
        &state->data.vegetation_layers[i];
    char value[128];
    snprintf(label, sizeof(label), "Vegetation %u", i);
    snprintf(value, sizeof(value), "%s  (density %.2f)",
        layer->type_name, (double)layer->density_scale);
    ldk_editor_ui_row_label(editor, label, 0u, value);
  }
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
