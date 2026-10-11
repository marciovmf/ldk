/* Texture atlas authoring tool. Atlas data never owns or duplicates image pixels. */
#include "ldk_editor_internal.h"
#include "ldk_ui_drag_n_drop.h"
#include <ldk_texture_atlas.h>
#include <module/ldk_asset_source.h>
#include <stdx/stdx_io.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef enum LDKAtlasDrag
{
  LDK_ATLAS_DRAG_NONE,
  LDK_ATLAS_DRAG_CREATE,
  LDK_ATLAS_DRAG_MOVE,
  LDK_ATLAS_DRAG_RESIZE,
  LDK_ATLAS_DRAG_PAN
} LDKAtlasDrag;

typedef enum LDKAtlasEditorTab
{
  LDK_ATLAS_EDITOR_TAB_SPRITES,
  LDK_ATLAS_EDITOR_TAB_GRID,
  LDK_ATLAS_EDITOR_TAB_COUNT
} LDKAtlasEditorTab;

/* Text fields are stored per sprite so several groups can remain expanded
 * without sharing input buffers or losing partially typed values. */
typedef struct LDKAtlasSpriteUIState
{
  u64 id;
  char name[LDK_TEXTURE_ATLAS_REGION_NAME_CAPACITY];
  char fields[6][32]; /* x, y, width, height, pivot X, pivot Y */
  bool expanded;
} LDKAtlasSpriteUIState;

typedef struct LDKAtlasEditorState
{
  LDKTextureAtlas atlas;
  XFSPath file;
  char asset_path[LDK_ASSET_PATH_MAX_LENGTH + 1u];
  LDKAtlasSpriteUIState *sprites;
  u32 sprite_count, sprite_capacity;
  LDKAtlasEditorTab active_tab;
  char grid[8][24]; /* size xy, spacing xy, margins xy, unused 2 */
  LDKResourceTexture texture;
  LDKAssetImage image_asset;
  float properties_width;
  float property_label_width;
  LDKUIPoint property_scroll[LDK_ATLAS_EDITOR_TAB_COUNT];
  LDKUIRect canvas_rect;
  float zoom;
  float offset_x, offset_y;
  float drag_start_x, drag_start_y;
  float drag_pan_x, drag_pan_y;
  LDKTextureAtlasRegion drag_original;
  u64 selected;
  u32 image_width, image_height;
  u32 new_name_serial;
  LDKAtlasDrag drag;
  bool loaded;
  bool dirty;
  bool read_only;
  bool grid_preview;
} LDKAtlasEditorState;

static LDKAtlasEditorState s_atlas_editor;

static float s_atlas_clamp(float value, float low, float high)
{
  return value < low ? low : value > high ? high : value;
}

static void s_atlas_texture_release(LDKRenderer *renderer)
{
  if (renderer && ldk_renderer_texture_is_valid(renderer, s_atlas_editor.texture))
  {
    ldk_renderer_texture_destroy(renderer, s_atlas_editor.texture);
  }
  s_atlas_editor.texture = ldk_renderer_texture_null();
  s_atlas_editor.image_asset = ldk_asset_image_null();
  s_atlas_editor.image_width = s_atlas_editor.image_height = 0;
}

/* Does not mutate the stored texture path unless a real image was found. */
static bool s_atlas_texture_load(LDKEditorContext *editor, const char *logical)
{
  LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (!editor || !editor->renderer || !assets || !logical || !logical[0])
  {
    return false;
  }
  LDKAssetImage image = ldk_asset_manager_image_load_shared(assets, logical);
  const LDKAssetImageData *data =
      ldk_asset_manager_image_get_const(assets, image);
  if (!data || !data->image || data->is_missing)
  {
    return false;
  }
  LDKRendererTextureOptions options;
  ldk_renderer_texture_options_defaults(&options);
  options.min_filter = LDK_RHI_FILTER_NEAREST;
  options.mag_filter = LDK_RHI_FILTER_NEAREST;
  LDKResourceTexture resource = ldk_renderer_texture_create_from_image(
      editor->renderer, data->image, &options);
  if (!ldk_renderer_texture_is_valid(editor->renderer, resource))
  {
    return false;
  }
  s_atlas_texture_release(editor->renderer);
  s_atlas_editor.texture = resource;
  s_atlas_editor.image_asset = image;
  s_atlas_editor.image_width = ldk_image_get_width(data->image);
  s_atlas_editor.image_height = ldk_image_get_height(data->image);
  return true;
}

static bool s_atlas_assign_texture(LDKEditorContext *editor,
    const char *logical)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  LDKAssetPath validated;
  if (!ldk_asset_path_set(&validated, logical) ||
      !s_atlas_texture_load(editor, validated.buf) ||
      !ldk_texture_atlas_texture_set(&state->atlas, validated.buf))
  {
    return false;
  }
  state->zoom = 0;
  state->dirty = true;
  return true;
}

static bool s_atlas_logical_from_file(LDKEditorContext *editor,
    const char *filename, char out[LDK_ASSET_PATH_MAX_LENGTH + 1u])
{
  XFSPath physical = {0};
  XFSPath relative = {0};
  LDKAssetPath path;
  const XFSPath *root = editor->project.loaded ?
      &editor->project.run_root_path : &editor->engine_runtree;
  if (!x_fs_path_set(&physical, filename) || !root->length ||
      !x_fs_path_relative_to(root, &physical, &relative) ||
      !ldk_asset_path_set(&path, relative.buf))
  {
    return false;
  }
  snprintf(out, LDK_ASSET_PATH_MAX_LENGTH + 1u, "%s", path.buf);
  return true;
}

static bool s_atlas_u32_parse(const char *text, u32 *out)
{
  char *end = NULL;
  if (!text || !*text || *text == '-' || !out)
  {
    return false;
  }
  errno = 0;
  unsigned long long number = strtoull(text, &end, 10);
  if (errno || end == text || *end || number > UINT32_MAX)
  {
    return false;
  }
  *out = (u32)number;
  return true;
}

static bool s_atlas_float_parse(const char *text, float *out)
{
  char *end = NULL;
  if (!text || !*text || !out)
  {
    return false;
  }
  errno = 0;
  float number = strtof(text, &end);
  if (errno || end == text || *end || !isfinite(number))
  {
    return false;
  }
  *out = number;
  return true;
}

static void s_atlas_sprite_fields_write(LDKAtlasSpriteUIState *sprite,
    const LDKTextureAtlasRegion *region)
{
  if (!sprite || !region)
  {
    return;
  }
  snprintf(sprite->name, sizeof(sprite->name), "%s", region->name);
  snprintf(sprite->fields[0], sizeof(sprite->fields[0]), "%u", region->x);
  snprintf(sprite->fields[1], sizeof(sprite->fields[1]), "%u", region->y);
  snprintf(sprite->fields[2], sizeof(sprite->fields[2]), "%u", region->width);
  snprintf(sprite->fields[3], sizeof(sprite->fields[3]), "%u", region->height);
  snprintf(sprite->fields[4], sizeof(sprite->fields[4]), "%.4f",
      (double)region->pivot_x);
  snprintf(sprite->fields[5], sizeof(sprite->fields[5]), "%.4f",
      (double)region->pivot_y);
}

static LDKAtlasSpriteUIState *s_atlas_sprite_state(u64 id, bool create)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  for (u32 i = 0; i < state->sprite_count; ++i)
  {
    if (state->sprites[i].id == id)
    {
      return &state->sprites[i];
    }
  }
  if (!create || !id)
  {
    return NULL;
  }
  const LDKTextureAtlasRegion *region =
      ldk_texture_atlas_region_find_const(&state->atlas, id);
  if (!region)
  {
    return NULL;
  }
  if (state->sprite_count == state->sprite_capacity)
  {
    u32 capacity = state->sprite_capacity ? state->sprite_capacity * 2u : 16u;
    LDKAtlasSpriteUIState *sprites = realloc(state->sprites,
        (size_t)capacity * sizeof(*sprites));
    if (!sprites)
    {
      return NULL;
    }
    state->sprites = sprites;
    state->sprite_capacity = capacity;
  }
  LDKAtlasSpriteUIState *sprite = &state->sprites[state->sprite_count++];
  *sprite = (LDKAtlasSpriteUIState){0};
  sprite->id = id;
  s_atlas_sprite_fields_write(sprite, region);
  return sprite;
}

static void s_atlas_sprite_state_remove(u64 id)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  for (u32 i = 0; i < state->sprite_count; ++i)
  {
    if (state->sprites[i].id == id)
    {
      memmove(&state->sprites[i], &state->sprites[i + 1],
          (size_t)(state->sprite_count - i - 1u) * sizeof(*state->sprites));
      --state->sprite_count;
      break;
    }
  }
}

static void s_atlas_region_fields_sync(void)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  const LDKTextureAtlasRegion *region =
      ldk_texture_atlas_region_find_const(&state->atlas, state->selected);
  if (region)
  {
    s_atlas_sprite_fields_write(
        s_atlas_sprite_state(region->id, true), region);
  }
}

static void s_atlas_select(u64 id)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  if (state->selected != id)
  {
    state->selected = id;
    s_atlas_region_fields_sync();
  }
  if (id)
  {
    state->active_tab = LDK_ATLAS_EDITOR_TAB_SPRITES;
    LDKAtlasSpriteUIState *sprite = s_atlas_sprite_state(id, true);
    if (sprite)
    {
      sprite->expanded = true;
    }
  }
}

static void s_atlas_editor_reset(LDKEditorContext *editor)
{
  free(s_atlas_editor.sprites);
  s_atlas_texture_release(editor ? editor->renderer : NULL);
  ldk_texture_atlas_clear(&s_atlas_editor.atlas);
  memset(&s_atlas_editor, 0, sizeof(s_atlas_editor));
  ldk_texture_atlas_init(&s_atlas_editor.atlas);
  s_atlas_editor.texture = ldk_renderer_texture_null();
  s_atlas_editor.zoom = 0.0f;
  s_atlas_editor.grid_preview = true;
  s_atlas_editor.properties_width = 260.0f;
  s_atlas_editor.active_tab = LDK_ATLAS_EDITOR_TAB_SPRITES;
  s_atlas_editor.property_label_width = 125.0f;
  const char *initial[] = {"32", "32", "0", "0", "0", "0"};
  for (u32 i = 0; i < 6; ++i)
  {
    snprintf(s_atlas_editor.grid[i], sizeof(s_atlas_editor.grid[i]), "%s",
        initial[i]);
  }
}

void ldki_editor_texture_atlas_terminate(LDKEditorContext *editor)
{
  s_atlas_editor_reset(editor);
}

bool ldki_editor_texture_atlas_project_close(LDKEditorContext *editor)
{
  if (s_atlas_editor.loaded && s_atlas_editor.dirty &&
      !ldk_os_dialog_show_yes_no(editor->window, "Unsaved atlas",
          "Discard unsaved texture atlas changes and close the project?"))
  {
    return false;
  }
  ldki_editor_window_hide(LDK_EDITOR_WINDOW_TEXTURE_ATLAS);
  ldki_editor_texture_atlas_terminate(editor);
  return true;
}

bool ldki_editor_texture_atlas_open(LDKEditorContext *editor,
    const char *filename, const char *logical_path, bool read_only)
{
  if (!editor || !filename || !logical_path || !*logical_path)
  {
    return false;
  }
  if (s_atlas_editor.loaded && s_atlas_editor.dirty)
  {
    if (!ldk_os_dialog_show_yes_no(editor->window,
            "Unsaved atlas", "Discard unsaved atlas changes?"))
    {
      return false;
    }
  }
  LDKTextureAtlas candidate;
  ldk_texture_atlas_init(&candidate);
  bool success = false;
  if (read_only)
  {
    LDKAssetManager *manager = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    LDKAssetTextureAtlas asset =
        ldk_asset_manager_texture_atlas_load_shared(manager, logical_path);
    const LDKAssetTextureAtlasData *data =
        ldk_asset_manager_texture_atlas_get_const(manager, asset);
    success = data && ldk_texture_atlas_copy(&candidate, &data->atlas);
  }
  else
  {
    success = ldk_texture_atlas_load(&candidate, filename);
  }
  if (!success)
  {
    ldk_texture_atlas_clear(&candidate);
    return false;
  }
  s_atlas_editor_reset(editor);
  s_atlas_editor.atlas = candidate;
  s_atlas_editor.loaded = true;
  s_atlas_editor.read_only = read_only;
  snprintf(s_atlas_editor.asset_path, sizeof(s_atlas_editor.asset_path),
      "%s", logical_path);
  if (!read_only)
  {
    (void)x_fs_path_set(&s_atlas_editor.file, filename);
  }
  if (s_atlas_editor.atlas.texture_path.buf[0] &&
      !s_atlas_texture_load(editor, s_atlas_editor.atlas.texture_path.buf))
  {
    ldki_editor_log_error(editor,
        "Atlas loaded but its texture image is unavailable.");
  }
  if (!ldki_editor_window_show(LDK_EDITOR_WINDOW_TEXTURE_ATLAS))
  {
    return false;
  }
  (void)ldki_editor_window_activate(LDK_EDITOR_WINDOW_TEXTURE_ATLAS);
  return true;
}

static void s_atlas_error(LDKEditorContext *editor, const char *message)
{
  ldki_editor_log_error(editor, message);
}

static bool s_atlas_save(LDKEditorContext *editor)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  if (!state->loaded || state->read_only || !state->file.length ||
      !ldk_texture_atlas_save(&state->atlas, state->file.buf))
  {
    s_atlas_error(editor, "Failed to save texture atlas.");
    return false;
  }
  state->dirty = false;
  LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if (assets && assets->source &&
      !ldk_asset_manager_texture_atlas_reload(assets, state->asset_path))
  {
    s_atlas_error(editor, "Atlas saved, but the cached asset could not reload.");
  }
  return true;
}

/* Cancel restores the last saved atlas instead of closing the docked tool. */
static bool s_atlas_discard(LDKEditorContext *editor)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  LDKTextureAtlas original;
  if (!state->loaded || state->read_only || !state->file.length)
  {
    return false;
  }
  ldk_texture_atlas_init(&original);
  if (!ldk_texture_atlas_load(&original, state->file.buf))
  {
    ldk_texture_atlas_clear(&original);
    s_atlas_error(editor, "Failed to reload saved texture atlas.");
    return false;
  }
  s_atlas_texture_release(editor->renderer);
  ldk_texture_atlas_clear(&state->atlas);
  state->atlas = original;
  free(state->sprites);
  state->sprites = NULL;
  state->sprite_count = state->sprite_capacity = 0;
  state->selected = 0;
  state->drag = LDK_ATLAS_DRAG_NONE;
  state->dirty = false;
  state->new_name_serial = 0;
  state->zoom = 0.0f;
  s_atlas_region_fields_sync();
  if (state->atlas.texture_path.buf[0] &&
      !s_atlas_texture_load(editor, state->atlas.texture_path.buf))
  {
    s_atlas_error(editor, "Atlas restored, but its texture is unavailable.");
  }
  return true;
}

static bool s_atlas_auto_region(LDKEditorContext *editor, u32 x, u32 y,
    u32 w, u32 h)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  if (!w || !h || state->read_only)
  {
    return false;
  }
  char name[LDK_TEXTURE_ATLAS_REGION_NAME_CAPACITY];
  u64 id;
  do
  {
    if (state->new_name_serial == UINT32_MAX)
    {
      return false;
    }
    snprintf(name, sizeof(name), "sprite_%04u", state->new_name_serial++);
    bool duplicate = false;
    for (u32 i = 0; i < state->atlas.region_count; ++i)
    {
      if (strcmp(state->atlas.regions[i].name, name) == 0)
      {
        duplicate = true;
        break;
      }
    }
    if (!duplicate)
    {
      break;
    }
  } while (true);
  if (!ldk_texture_atlas_region_add(&state->atlas, name, x, y, w, h, &id))
  {
    s_atlas_error(editor, "Failed to add atlas region.");
    return false;
  }
  s_atlas_select(id);
  state->dirty = true;
  return true;
}

static bool s_atlas_rect_contains(LDKUIRect rect, float x, float y)
{
  return x >= rect.x && y >= rect.y && x < rect.x + rect.w &&
      y < rect.y + rect.h;
}

static LDKUIRect s_atlas_to_screen(const LDKAtlasEditorState *state,
    const LDKTextureAtlasRegion *region, LDKUIRect image)
{
  (void)state;
  return (LDKUIRect){image.x + region->x * s_atlas_editor.zoom,
      image.y + region->y * s_atlas_editor.zoom,
      region->width * s_atlas_editor.zoom,
      region->height * s_atlas_editor.zoom};
}

static void s_atlas_draw_rect(LDKUIContext *ui, LDKUIRect rect, rgba32 color)
{
  if (rect.w > 0 && rect.h > 0)
  {
    ldk_ui_widget_gradient(ui, 0, color, color, color, color, rect);
  }
}

static void s_atlas_draw_outline(LDKUIContext *ui, LDKUIRect rect, rgba32 color)
{
  s_atlas_draw_rect(ui, (LDKUIRect){rect.x, rect.y, rect.w, 2}, color);
  s_atlas_draw_rect(ui, (LDKUIRect){rect.x, rect.y + rect.h - 2, rect.w, 2}, color);
  s_atlas_draw_rect(ui, (LDKUIRect){rect.x, rect.y, 2, rect.h}, color);
  s_atlas_draw_rect(ui, (LDKUIRect){rect.x + rect.w - 2, rect.y, 2, rect.h}, color);
}

static bool s_atlas_image_xy(LDKAtlasEditorState *state,
    LDKUIRect image, float screen_x, float screen_y, u32 *out_x, u32 *out_y)
{
  if (!s_atlas_rect_contains(image, screen_x, screen_y) || state->zoom <= 0)
  {
    return false;
  }
  *out_x = (u32)s_atlas_clamp(floorf((screen_x - image.x) / state->zoom),
      0, (float)(state->image_width - 1));
  *out_y = (u32)s_atlas_clamp(floorf((screen_y - image.y) / state->zoom),
      0, (float)(state->image_height - 1));
  return true;
}

static void s_atlas_canvas(LDKEditorContext *editor, LDKUIContext *ui,
    float height)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  float w = ui->current_layout->content_rect.w;
  float h = height;
  ldk_ui_set_next_size(ui, ldk_ui_px(fmaxf(10, w)),
      ldk_ui_px(fmaxf(10, h)));
  ldk_ui_spacer(ui);
  LDKUIRect canvas = ldk_ui_last_rect(ui);
  state->canvas_rect = canvas;
  LDKUIRect old_clip = ui->clip_rect;
  ui->clip_rect = ldk_rectf_intersect(&ui->clip_rect, &canvas);
  s_atlas_draw_rect(ui, canvas, ui->theme.colors[LDK_UI_COLOR_PANEL_BG]);
  LDKMouseState *mouse = (LDKMouseState *)ui->mouse;
  if (!state->read_only && mouse && ui->current_window &&
      ui->hovered_window_id == ui->current_window->id &&
      ldk_os_mouse_button_up(mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    LDKPoint cursor = ldk_os_mouse_cursor(mouse);
    if (s_atlas_rect_contains(canvas, (float)cursor.x, (float)cursor.y) &&
        s_atlas_rect_contains(old_clip, (float)cursor.x, (float)cursor.y))
    {
      u32 type = 0;
      XSmallstr payload = {0};
      if (ldk_ui_drag_n_drop_payload_get_and_remove(&type, &payload))
      {
        char logical[LDK_ASSET_PATH_MAX_LENGTH + 1u] = {0};
        bool valid = type == LDK_EDITOR_DRAG_N_DROP_PAYLOAD_ASSET_PATH ?
            snprintf(logical, sizeof(logical), "%s", payload.buf) > 0 :
            type == LDK_EDITOR_DRAG_N_DROP_PAYLOAD_FILE_PATH &&
            s_atlas_logical_from_file(editor, payload.buf, logical);
        if (!valid || !s_atlas_assign_texture(editor, logical))
        {
          s_atlas_error(editor, "Dropped file is not a readable texture asset.");
        }
      }
    }
  }
  if (!state->image_width || !state->image_height)
  {
    ldk_ui_widget_label(ui, 0,
        "Assign a texture asset path to begin editing regions.",
        (LDKUIRect){canvas.x + 12, canvas.y + 12, canvas.w - 24, 28});
    ui->clip_rect = old_clip;
    return;
  }
  if (state->zoom <= 0.0f)
  {
    state->zoom = fminf((canvas.w - 24) / state->image_width,
        (canvas.h - 24) / state->image_height);
    state->zoom = s_atlas_clamp(state->zoom, 0.01f, 16.0f);
    state->offset_x = state->offset_y = 0;
  }
  LDKUIRect image = {canvas.x + (canvas.w - state->image_width * state->zoom) * 0.5f + state->offset_x,
      canvas.y + (canvas.h - state->image_height * state->zoom) * 0.5f + state->offset_y,
      state->image_width * state->zoom, state->image_height * state->zoom};
  if (mouse)
  {
    LDKPoint cursor = ldk_os_mouse_cursor(mouse);
    bool inside = s_atlas_rect_contains(canvas, (float)cursor.x, (float)cursor.y) &&
        s_atlas_rect_contains(old_clip, (float)cursor.x, (float)cursor.y);
    if (inside)
    {
      i32 wheel = ldk_os_mouse_wheel_delta(mouse);
      if (wheel)
      {
        float old_zoom = state->zoom;
        state->zoom = s_atlas_clamp(old_zoom * powf(1.1f, (float)wheel / 120.0f),
            0.01f, 32.0f);
        /* Preserve the source pixel beneath the cursor while zooming. */
        state->offset_x += ((float)cursor.x - image.x) * (1 - state->zoom / old_zoom) +
            (canvas.w - state->image_width * old_zoom) * 0.5f -
            (canvas.w - state->image_width * state->zoom) * 0.5f;
        state->offset_y += ((float)cursor.y - image.y) * (1 - state->zoom / old_zoom) +
            (canvas.h - state->image_height * old_zoom) * 0.5f -
            (canvas.h - state->image_height * state->zoom) * 0.5f;
        image.x = canvas.x + (canvas.w - state->image_width * state->zoom) * 0.5f + state->offset_x;
        image.y = canvas.y + (canvas.h - state->image_height * state->zoom) * 0.5f + state->offset_y;
        image.w = state->image_width * state->zoom;
        image.h = state->image_height * state->zoom;
      }
    }
    if (inside && ldk_os_mouse_button_down(mouse, LDK_MOUSE_BUTTON_MIDDLE))
    {
      state->drag = LDK_ATLAS_DRAG_PAN;
      state->drag_start_x = (float)cursor.x;
      state->drag_start_y = (float)cursor.y;
      state->drag_pan_x = state->offset_x;
      state->drag_pan_y = state->offset_y;
    }
    if (state->drag == LDK_ATLAS_DRAG_PAN)
    {
      if (!ldk_os_mouse_button_is_pressed(mouse, LDK_MOUSE_BUTTON_MIDDLE))
      {
        state->drag = LDK_ATLAS_DRAG_NONE;
      }
      else
      {
        state->offset_x = state->drag_pan_x + (float)cursor.x - state->drag_start_x;
        state->offset_y = state->drag_pan_y + (float)cursor.y - state->drag_start_y;
        image.x = canvas.x + (canvas.w - image.w) * 0.5f + state->offset_x;
        image.y = canvas.y + (canvas.h - image.h) * 0.5f + state->offset_y;
      }
    }
    if (inside && state->drag == LDK_ATLAS_DRAG_NONE &&
        ldk_os_mouse_button_down(mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      u32 px, py;
      if (s_atlas_image_xy(state, image, (float)cursor.x, (float)cursor.y, &px, &py))
      {
        state->drag_start_x = (float)px;
        state->drag_start_y = (float)py;
        state->drag = state->read_only ? LDK_ATLAS_DRAG_NONE : LDK_ATLAS_DRAG_CREATE;
        for (u32 i = state->atlas.region_count; i > 0; --i)
        {
          LDKTextureAtlasRegion *r = &state->atlas.regions[i-1];
          LDKUIRect rect = s_atlas_to_screen(state, r, image);
          if (s_atlas_rect_contains(rect, (float)cursor.x, (float)cursor.y))
          {
            s_atlas_select(r->id);
            state->drag_original = *r;
            float handle_x = rect.x + rect.w;
            float handle_y = rect.y + rect.h;
            if (!state->read_only)
            {
              state->drag = fabsf(cursor.x - handle_x) < 9 &&
                  fabsf(cursor.y - handle_y) < 9 ?
                  LDK_ATLAS_DRAG_RESIZE : LDK_ATLAS_DRAG_MOVE;
            }
            break;
          }
        }
        if (state->drag == LDK_ATLAS_DRAG_CREATE)
        {
          s_atlas_select(0);
        }
      }
    }
    if (state->drag == LDK_ATLAS_DRAG_CREATE ||
        state->drag == LDK_ATLAS_DRAG_MOVE ||
        state->drag == LDK_ATLAS_DRAG_RESIZE)
    {
      if (ldk_os_mouse_button_is_pressed(mouse, LDK_MOUSE_BUTTON_LEFT) ||
          ldk_os_mouse_button_up(mouse, LDK_MOUSE_BUTTON_LEFT))
      {
        u32 px = (u32)s_atlas_clamp(floorf(((float)cursor.x - image.x) / state->zoom),
            0, (float)state->image_width);
        u32 py = (u32)s_atlas_clamp(floorf(((float)cursor.y - image.y) / state->zoom),
            0, (float)state->image_height);
        if (state->drag == LDK_ATLAS_DRAG_CREATE)
        {
          u32 x0 = (u32)fminf(state->drag_start_x, (float)px);
          u32 y0 = (u32)fminf(state->drag_start_y, (float)py);
          u32 x1 = (u32)fmaxf(state->drag_start_x, (float)px);
          u32 y1 = (u32)fmaxf(state->drag_start_y, (float)py);
          if (ldk_os_mouse_button_up(mouse, LDK_MOUSE_BUTTON_LEFT) &&
              x1 > x0 && y1 > y0)
          {
            (void)s_atlas_auto_region(editor, x0, y0, x1 - x0, y1 - y0);
          }
        }
        else
        {
          LDKTextureAtlasRegion *r =
              ldk_texture_atlas_region_find(&state->atlas, state->selected);
          if (r)
          {
            if (state->drag == LDK_ATLAS_DRAG_MOVE)
            {
              i64 dx = (i64)px - (i64)state->drag_start_x;
              i64 dy = (i64)py - (i64)state->drag_start_y;
              if (r->width <= state->image_width &&
                  r->height <= state->image_height)
              {
                r->x = (u32)s_atlas_clamp(
                    (float)((i64)state->drag_original.x + dx),
                    0, (float)(state->image_width - r->width));
                r->y = (u32)s_atlas_clamp(
                    (float)((i64)state->drag_original.y + dy),
                    0, (float)(state->image_height - r->height));
              }
            }
            else
            {
              if (r->x < state->image_width && r->y < state->image_height)
              {
                r->width = (u32)s_atlas_clamp((float)px - r->x,
                    1, (float)(state->image_width - r->x));
                r->height = (u32)s_atlas_clamp((float)py - r->y,
                    1, (float)(state->image_height - r->y));
              }
            }
            if (memcmp(r, &state->drag_original, sizeof(*r)) != 0)
            {
              state->dirty = true;
              s_atlas_region_fields_sync();
            }
          }
        }
      }
      if (ldk_os_mouse_button_up(mouse, LDK_MOUSE_BUTTON_LEFT))
      {
        state->drag = LDK_ATLAS_DRAG_NONE;
      }
    }
  }
  if (ldk_renderer_texture_is_valid(editor->renderer, state->texture))
  {
    ldk_ui_widget_image(ui, 0,
        ldk_renderer_texture_ui_handle(editor->renderer, state->texture),
        (LDKUIRect){0, 0, 1, 1}, image);
  }
  for (u32 i = 0; i < state->atlas.region_count; ++i)
  {
    const LDKTextureAtlasRegion *r = &state->atlas.regions[i];
    LDKUIRect rect = s_atlas_to_screen(state, r, image);
    bool selected = r->id == state->selected;
    s_atlas_draw_outline(ui, rect, selected ?
        ui->theme.colors[LDK_UI_COLOR_FOCUS] :
        ui->theme.colors[LDK_UI_COLOR_TEXT]);
    if (selected)
    {
      rgba32 color = ui->theme.colors[LDK_UI_COLOR_FOCUS];
      s_atlas_draw_rect(ui, (LDKUIRect){rect.x + rect.w - 5,
          rect.y + rect.h - 5, 10, 10}, color);
      float pivot_x = rect.x + rect.w * r->pivot_x;
      float pivot_y = rect.y + rect.h * r->pivot_y;
      s_atlas_draw_rect(ui, (LDKUIRect){pivot_x - 5, pivot_y, 10, 1}, color);
      s_atlas_draw_rect(ui, (LDKUIRect){pivot_x, pivot_y - 5, 1, 10}, color);
    }
  }
  if (state->drag == LDK_ATLAS_DRAG_CREATE && mouse)
  {
    LDKPoint cursor = ldk_os_mouse_cursor(mouse);
    u32 px = (u32)s_atlas_clamp(floorf(((float)cursor.x - image.x) / state->zoom),
        0, (float)state->image_width);
    u32 py = (u32)s_atlas_clamp(floorf(((float)cursor.y - image.y) / state->zoom),
        0, (float)state->image_height);
    float x0 = fminf(state->drag_start_x, (float)px);
    float y0 = fminf(state->drag_start_y, (float)py);
    float x1 = fmaxf(state->drag_start_x, (float)px);
    float y1 = fmaxf(state->drag_start_y, (float)py);
    s_atlas_draw_outline(ui, (LDKUIRect){image.x + x0 * state->zoom,
        image.y + y0 * state->zoom, (x1 - x0) * state->zoom,
        (y1 - y0) * state->zoom}, ui->theme.colors[LDK_UI_COLOR_FOCUS]);
  }
  if (state->grid_preview && state->active_tab == LDK_ATLAS_EDITOR_TAB_GRID)
  {
    u32 v[6];
    bool valid = true;
    for (u32 i = 0; i < 6; ++i)
    {
      if (!s_atlas_u32_parse(state->grid[i], &v[i])) valid = false;
    }
    if (valid && v[0] && v[1] &&
        (u64)v[4] * 2u + v[0] <= state->image_width &&
        (u64)v[5] * 2u + v[1] <= state->image_height)
    {
      u64 cols = 1u + ((u64)state->image_width - 2u*v[4] - v[0]) /
          ((u64)v[0] + v[2]);
      u64 rows = 1u + ((u64)state->image_height - 2u*v[5] - v[1]) /
          ((u64)v[1] + v[3]);
      if (rows * cols <= 4096)
      {
        for (u64 row = 0; row < rows; ++row)
          for (u64 col = 0; col < cols; ++col)
            s_atlas_draw_outline(ui, (LDKUIRect){
                image.x + (float)(v[4] + col*((u64)v[0]+v[2]))*state->zoom,
                image.y + (float)(v[5] + row*((u64)v[1]+v[3]))*state->zoom,
                v[0]*state->zoom, v[1]*state->zoom},
                ui->theme.colors[LDK_UI_COLOR_SEPARATOR]);
      }
    }
  }
  ui->clip_rect = old_clip;
}

/* Inspector-style property row: label | draggable divider | control.
 * The divider width is shared between the Region and Grid sections. */
static void s_atlas_property_row_begin(LDKUIContext *ui, const char *label)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  const float min_label = 60.0f;
  const float min_control = 80.0f;
  float spacing, max_label, label_width;

  if (!isfinite(state->property_label_width) ||
      state->property_label_width <= 0.0f)
  {
    state->property_label_width = 125.0f;
  }

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  spacing = ui->current_layout != NULL ? ui->current_layout->spacing :
      LDK_UI_DEFAULT_SPACING;
  max_label = ui->current_layout != NULL ?
      ui->current_layout->content_rect.w - min_control - spacing :
      state->property_label_width;
  if (max_label < min_label)
  {
    max_label = min_label;
  }
  state->property_label_width = s_atlas_clamp(
      state->property_label_width, min_label, max_label);
  label_width = fmaxf(1.0f, state->property_label_width - spacing);
  ldk_ui_set_next_width(ui, ldk_ui_px(label_width));
  ldk_ui_label(ui, label);
  state->property_label_width = ldk_ui_resize_handle_vertical(ui,
      state->property_label_width, min_label, max_label);
}

/* Edit the fields of one expanded sprite, independently of selection. */
static void s_atlas_region_properties(LDKEditorContext *editor,
    LDKUIContext *ui, u64 id)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  LDKTextureAtlasRegion *r =
      ldk_texture_atlas_region_find(&state->atlas, id);
  LDKAtlasSpriteUIState *sprite = s_atlas_sprite_state(id, false);
  if (!r || !sprite)
  {
    return;
  }
  ldk_ui_label(ui, "Region");
  ldk_ui_begin_disabled(ui, state->read_only);
  s_atlas_property_row_begin(ui, "Name");
  u32 change = ldk_ui_input_box(ui, sprite->name,
      sizeof(sprite->name));
  ldk_ui_end_horizontal(ui);
  if (change & LDK_UI_INPUT_BOX_COMMITTED)
  {
    if (ldk_texture_atlas_region_rename(&state->atlas, r->id,
            sprite->name))
    {
      state->dirty = true;
    }
    else
    {
      s_atlas_error(editor, "Region names must be non-empty and unique.");
      s_atlas_sprite_fields_write(sprite, r);
    }
  }
  const char *labels[] = {"X", "Y", "Width", "Height", "Pivot X (0..1)", "Pivot Y (0..1)"};
  for (u32 i = 0; i < 6; ++i)
  {
    s_atlas_property_row_begin(ui, labels[i]);
    u32 result = ldk_ui_input_box(ui, sprite->fields[i],
        sizeof(sprite->fields[i]));
    ldk_ui_end_horizontal(ui);
    if (result & LDK_UI_INPUT_BOX_COMMITTED)
    {
      LDKTextureAtlasRegion edited = *r;
      bool valid = true;
      if (i < 4)
      {
        u32 number;
        valid = s_atlas_u32_parse(sprite->fields[i], &number);
        if (valid)
        {
          if (i == 0) edited.x = number;
          if (i == 1) edited.y = number;
          if (i == 2) edited.width = number;
          if (i == 3) edited.height = number;
          valid = edited.width && edited.height &&
              (u64)edited.x + edited.width <= UINT32_MAX &&
              (u64)edited.y + edited.height <= UINT32_MAX;
        }
      }
      else
      {
        float number;
        valid = s_atlas_float_parse(sprite->fields[i], &number) &&
            number >= 0.0f && number <= 1.0f;
        if (valid)
        {
          if (i == 4) edited.pivot_x = number;
          else edited.pivot_y = number;
        }
      }
      if (valid)
      {
        *r = edited;
        state->dirty = true;
      }
      else
      {
        s_atlas_error(editor, "Invalid region field value.");
      }
      s_atlas_sprite_fields_write(sprite, r);
    }
  }

  /* Keep the existing duplicate operation available inside the group. */
  if (ldk_ui_button(ui, "Duplicate"))
  {
    u32 x = r->x, y = r->y, w = r->width, h = r->height;
    float px = r->pivot_x, py = r->pivot_y;
    if (s_atlas_auto_region(editor, x, y, w, h))
    {
      LDKTextureAtlasRegion *copy =
          ldk_texture_atlas_region_find(&state->atlas, state->selected);
      if (copy)
      {
        copy->pivot_x = px;
        copy->pivot_y = py;
        s_atlas_region_fields_sync();
      }
    }
  }
  ldk_ui_end_disabled(ui);
}

static void s_atlas_grid_controls(LDKEditorContext *editor, LDKUIContext *ui)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  const char *labels[] = {"Cell width", "Cell height", "Spacing X", "Spacing Y",
      "Margin X", "Margin Y"};
  s_atlas_property_row_begin(ui, "Preview grid");
  ldk_ui_set_next_weight(ui, 0.0f);
  state->grid_preview = ldk_ui_toggle(ui, state->grid_preview);
  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(ui);
  ldk_ui_begin_disabled(ui, state->read_only || !state->image_width);
  for (u32 i = 0; i < 6; ++i)
  {
    s_atlas_property_row_begin(ui, labels[i]);
    ldk_ui_input_box(ui, state->grid[i], sizeof(state->grid[i]));
    ldk_ui_end_horizontal(ui);
  }
  if (ldk_ui_button(ui, "Add Grid Regions"))
  {
    u32 values[6] = {0};
    bool valid = true;
    for (u32 i = 0; i < 6; ++i)
    {
      valid = s_atlas_u32_parse(state->grid[i], &values[i]) && valid;
    }
    if (valid && ldk_texture_atlas_grid_append(&state->atlas,
            state->image_width, state->image_height,
            values[0], values[1], values[2], values[3], values[4], values[5]))
    {
      state->dirty = true;
      state->grid_preview = false;
    }
    else
    {
      s_atlas_error(editor, "Invalid grid: check cell size, spacing and margins.");
    }
  }
  ldk_ui_end_disabled(ui);
}

/* Sprite list with independent collapsible groups and a delete action in
 * each header. The stable region ID keeps expansion state across renames. */
static void s_atlas_sprites_panel(LDKEditorContext *editor, LDKUIContext *ui)
{
  LDKAtlasEditorState *state = &s_atlas_editor;
  if (!state->atlas.region_count)
  {
    ldk_ui_label(ui, "Drag on the texture or generate a grid to add sprites.");
    return;
  }

  LDKUIIcon delete_icon = {0};
  delete_icon.size = ldk_sizef(16.0f, 16.0f);
  delete_icon.texture = ldk_renderer_texture_ui_handle(
      editor->renderer, editor->ui_atlas);
  delete_icon.color = ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT];
  delete_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_DELETE];

  ldk_ui_push_id_cstr(ui, "atlas_sprites");
  for (u32 i = 0; i < state->atlas.region_count;)
  {
    u64 id = state->atlas.regions[i].id;
    LDKAtlasSpriteUIState *sprite = s_atlas_sprite_state(id, true);
    if (!sprite)
    {
      ++i;
      continue;
    }

    bool was_expanded = sprite->expanded;
    u32 flags = id == state->selected ? LDK_UI_TREE_NODE_SELECTED :
        LDK_UI_TREE_NODE_NONE;

    ldk_ui_push_id_u32(ui, (u32)id);
    ldk_ui_push_id_u32(ui, (u32)(id >> 32));
    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_weight(ui, 1.0f);
    u32 result = ldk_ui_tree_node_ex(ui, state->atlas.regions[i].name,
        (LDKUIIcon){0}, was_expanded, 0, flags);

    ldk_ui_set_next_disabled(ui, state->read_only);
    ldk_ui_set_next_width(ui, ldk_ui_px(24.0f + LDK_UI_DEFAULT_SPACING));
    bool remove = ldk_ui_icon_button(ui, delete_icon, NULL);
    ldk_ui_end_horizontal(ui);

    if (remove && !state->read_only &&
        ldk_texture_atlas_region_remove(&state->atlas, id))
    {
      if (state->selected == id)
      {
        state->selected = 0;
      }
      state->dirty = true;
      s_atlas_sprite_state_remove(id);
      ldk_ui_pop_id(ui);
      ldk_ui_pop_id(ui);
      continue; /* The following sprite moved into the current index. */
    }

    if (result & LDK_UI_TREE_NODE_RESULT_CLICKED)
    {
      s_atlas_select(id);
      sprite = s_atlas_sprite_state(id, false);
      if (sprite)
      {
        sprite->expanded = !was_expanded;
      }
    }

    if (sprite && sprite->expanded)
    {
      ldk_ui_push_id_cstr(ui, "fields");
      ldk_ui_set_next_width(ui, ldk_ui_fill());
      ldk_ui_begin_vertical(ui);
      ldk_ui_set_padding(ui, 8.0f);
      s_atlas_region_properties(editor, ui, id);
      ldk_ui_end_vertical(ui);
      ldk_ui_pop_id(ui);
    }
    ldk_ui_pop_id(ui);
    ldk_ui_pop_id(ui);
    ++i;
  }
  ldk_ui_pop_id(ui);
}

void ldki_editor_texture_atlas_window_show(LDKEditor *opaque, void *data)
{
  (void)data;
  LDKEditorContext *editor = (LDKEditorContext *)opaque;
  LDKUIContext *ui = &editor->ui;
  LDKAtlasEditorState *state = &s_atlas_editor;
  if (!state->loaded)
  {
    ldk_ui_label(ui, "Double-click a .atlas file in the Project Explorer.");
    return;
  }

  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_set_next_height(ui, ldk_ui_px(27.0f));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_label(ui, state->read_only ? "Package (read-only):" : "Atlas:");
  /* The atlas name is a reveal control, matching the asset fields in the
   * Inspector. It works for filesystem files and mounted packages. */
  ldk_ui_set_next_weight(ui, 0.0f);
  float name_width = ldk_ttf_measure_text_cstr(
      ui->font, state->asset_path).w + 20.0f;
  float max_name_width = ui->current_layout != NULL ?
      fmaxf(60.0f, ui->current_layout->content_rect.w - 145.0f) :
      name_width;
  ldk_ui_set_next_width(ui, ldk_ui_px(
      s_atlas_clamp(name_width, 60.0f, max_name_width)));
  if (ldk_ui_button_flat(ui, state->asset_path))
  {
    if (!ldki_editor_file_explorer_reveal_asset(editor, state->asset_path))
    {
      s_atlas_error(editor, "Could not locate atlas in Project Explorer.");
    }
  }
  if (state->dirty)
  {
    ldk_ui_set_next_weight(ui, 0.0f);
    ldk_ui_label(ui, "(modified)");
  }
  ldk_ui_spacer(ui);
  ldk_ui_set_next_width(ui, ldk_ui_px(64.0f));
  if (ldk_ui_button(ui, "Fit"))
  {
    state->zoom = 0;
  }
  ldk_ui_end_horizontal(ui);

  /* Share the image asset picker with the Inspector: drag/drop, reveal and
   * the system file dialog, with the same path validation rules. */
  LDKAssetImage image = state->image_asset;
  if (ldki_editor_image_asset_field(editor, "Texture asset", &image,
          state->read_only))
  {
    LDKAssetManager *assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
    LDKAssetHandle generic = {image.h};
    const LDKAssetInfo *info = assets ?
        ldk_asset_get_info_const(assets, generic) : NULL;
    if (!info || !info->asset_path.length ||
        !s_atlas_assign_texture(editor, info->asset_path.buf))
    {
      s_atlas_error(editor, "Failed to assign the selected image asset.");
    }
  }

  ldk_ui_set_next_weight(ui, 1.0f);
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_padding(ui, 0.0f);
  float available = ui->current_layout->content_rect.w;
  const float handle_width = 8.0f;
  const float gaps = LDK_UI_DEFAULT_SPACING * 2.0f + handle_width;
  float min_properties = fminf(190.0f, fmaxf(80.0f, available * 0.35f));
  float max_properties = fmaxf(min_properties, available - gaps - 160.0f);
  if (state->properties_width <= 0.0f)
  {
    state->properties_width = 260.0f;
  }
  state->properties_width = s_atlas_clamp(state->properties_width,
      min_properties, max_properties);
  float center_width = fmaxf(30.0f,
      available - state->properties_width - gaps);

  /* With sprites now in the right-side tab, the canvas uses the full height. */
  ldk_ui_set_next_width(ui, ldk_ui_px(center_width));
  ldk_ui_set_next_height(ui, ldk_ui_fill());
  ldk_ui_begin_vertical(ui);
  ldk_ui_set_padding(ui, 0.0f);
  s_atlas_canvas(editor, ui,
      fmaxf(30.0f, ui->current_layout->content_rect.h));
  ldk_ui_end_vertical(ui);

  /* The same splitter widget used by the animation editor. Moving this
   * handle resizes the properties panel while keeping the image flexible. */
  ldk_ui_set_next_width(ui, ldk_ui_px(handle_width));
  ldk_ui_set_next_height(ui, ldk_ui_fill());
  float adjusted_center = ldk_ui_resize_handle_vertical(ui, center_width,
      fmaxf(30.0f, available - max_properties - gaps),
      fmaxf(30.0f, available - min_properties - gaps));
  state->properties_width = s_atlas_clamp(
      available - adjusted_center - gaps, min_properties, max_properties);

  ldk_ui_set_next_width(ui, ldk_ui_fill());
  ldk_ui_set_next_height(ui, ldk_ui_fill());
  ldk_ui_begin_vertical(ui);
  static const LDKUITabBarItem tabs[] = {
      {.id = 1u, .label = "Sprite"},
      {.id = 2u, .label = "Grid"},
  };
  state->active_tab = (LDKAtlasEditorTab)ldk_ui_tab_bar(ui, tabs,
      LDK_ATLAS_EDITOR_TAB_COUNT, (u32)state->active_tab).active_index;
  u32 tab = (u32)state->active_tab;
  ldk_ui_set_next_weight(ui, 1.0f);
  ldk_ui_push_id_u32(ui, tab);
  state->property_scroll[tab] = ldk_ui_begin_scrollview(ui,
      state->property_scroll[tab],
      LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  if (state->active_tab == LDK_ATLAS_EDITOR_TAB_GRID)
  {
    s_atlas_grid_controls(editor, ui);
  }
  else
  {
    s_atlas_sprites_panel(editor, ui);
  }
  ldk_ui_end_scrollview(ui);
  ldk_ui_pop_id(ui);
  ldk_ui_end_vertical(ui);
  ldk_ui_end_horizontal(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_horizontal_line(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_begin_horizontal(ui);
  ldk_ui_spacer(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_begin_disabled(ui, state->read_only || !state->dirty);
  ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));
  if (ldk_ui_button(ui, "Save"))
  {
    (void)s_atlas_save(editor);
  }
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_set_next_width(ui, ldk_ui_px(80.0f));
  if (ldk_ui_button(ui, "Cancel"))
  {
    (void)s_atlas_discard(editor);
  }
  ldk_ui_end_disabled(ui);
  ldk_ui_end_horizontal(ui);
}
