#include "ldk_editor_internal.h"
#include "ldk_editor_scene_ops.h"
#include "ldk_editor_package_catalog.h"
#include "ldk_ui_drag_n_drop.h"
#include "module/ldk_ui.h"
#include "stdx/stdx_filesystem.h"
#include <ldk_image.h>
#include <ldk_material_io.h>
#include <ldk_package.h>
#include <ldk_scene.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_asset_source.h>
#include <stdx/stdx_io.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
  PROJECT_EXPLORER_INITIAL_CAPACITY = 32,
  PROJECT_EXPLORER_TREE_ICON_SIZE = 20,
  PROJECT_EXPLORER_MIN_ICON_SIZE = 20,
  PROJECT_EXPLORER_TILE_LABEL_LINE_COUNT = 2,
  PROJECT_EXPLORER_THUMBNAIL_SIZE = 128,
  PROJECT_EXPLORER_THUMBNAIL_CAPACITY = 128,
  PROJECT_EXPLORER_CONTEXT_POPUP_ID = 0x50454301u,
  PROJECT_EXPLORER_TREE_RENAME_INPUT_ID = 0x54524901u,
  PROJECT_EXPLORER_TILE_RENAME_INPUT_ID = 0x54494901u,
};

#define PROJECT_EXPLORER_DOUBLE_CLICK_SECONDS 0.35
#define PROJECT_EXPLORER_TREE_WIDTH_MIN 100.0f
#define PROJECT_EXPLORER_FILES_WIDTH_MIN 120.0f

typedef struct ProjectExplorerNode
{
  XFSPath path;
  XSmallstr name;
  u32 depth;
  u32 package_index;
  bool root;
  bool package_backed;
} ProjectExplorerNode;

typedef struct ProjectExplorerPackageExpandedPath
{
  LDKPackage *package;
  XFSPath path;
} ProjectExplorerPackageExpandedPath;

typedef struct ProjectExplorerEntry
{
  XFSPath path;
  XSmallstr name;
  LDKAssetPath asset_path;
  size_t size;
  time_t last_modified;
  LDKPackage *package;
  bool package_backed;
  bool is_directory;
} ProjectExplorerEntry;

typedef struct ProjectExplorerPackageMount
{
  XFSPath path;
  XSmallstr name;
  LDKPackage *package;
  LDKAssetSourcePackage *asset_source_package;
} ProjectExplorerPackageMount;

typedef struct ProjectExplorerTileResult
{
  bool clicked;
  bool pressed;
  bool right_clicked;
} ProjectExplorerTileResult;

typedef struct ProjectExplorerFileIcon
{
  const char *extension;
  LDKEditorIcon atlas_id;
} ProjectExplorerFileIcon;

typedef enum ProjectExplorerSurface
{
  PROJECT_EXPLORER_SURFACE_TREE,
  PROJECT_EXPLORER_SURFACE_FILES,
} ProjectExplorerSurface;

typedef struct ProjectExplorerContextTarget
{
  XFSPath path;
  bool is_directory;
  ProjectExplorerSurface surface;
} ProjectExplorerContextTarget;

typedef enum ProjectExplorerThumbnailStatus
{
  PROJECT_EXPLORER_THUMBNAIL_EMPTY,
  PROJECT_EXPLORER_THUMBNAIL_PENDING,
  PROJECT_EXPLORER_THUMBNAIL_READY,
  PROJECT_EXPLORER_THUMBNAIL_FAILED,
} ProjectExplorerThumbnailStatus;

typedef struct ProjectExplorerThumbnail
{
  XFSPath path;
  size_t size;
  time_t last_modified;
  LDKPackage *package;
  LDKResourceTexture texture;
  u32 width;
  u32 height;
  u64 last_used;
  u64 last_seen_frame;
  ProjectExplorerThumbnailStatus status;
} ProjectExplorerThumbnail;

typedef struct ProjectExplorerThumbnailCache
{
  LDKRenderer *renderer;
  ProjectExplorerThumbnail entries[PROJECT_EXPLORER_THUMBNAIL_CAPACITY];
  u64 frame;
  u64 access_counter;
} ProjectExplorerThumbnailCache;

typedef struct ProjectExplorerState
{
  LDKUIRect window_rect;
  LDKUIPoint tree_scroll;
  LDKUIPoint file_scroll;
  XFSPath root;
  XFSPath selected_directory;
  XFSPath selected_file;
  bool reveal_scroll_pending;
  LDKPackage *last_click_package;
  u64 last_click_ticks;
  XArray *expanded_paths;
  XArray *package_expanded_paths;
  XArray *stack;
  XArray *dirs;
  XArray *files;
  XArray *package_mounts;
  i32 selected_package;
  XSmallstr selected_package_directory;
  ProjectExplorerThumbnailCache thumbnails;
  ProjectExplorerContextTarget context_target;
  LDKUIRect rename_input_rect;
  LDKUIId rename_input_id;
  char rename_buffer[X_SMALLSTR_MAX_LENGTH + 1];
  bool rename_active;
  bool rename_focus_requested;
  bool rename_had_focus;
  bool root_expanded;
  float icon_size;
  float tree_width;
} ProjectExplorerState;

static ProjectExplorerState s_project_explorer_state = {
    .window_rect = {10.0f, 60.0f, 640.0f, 420.0f},
    .root_expanded = true,
    .selected_package = -1,
    .icon_size = 48.0f,
    .tree_width = LDK_EDITOR_FILE_EXPLORER_TREE_WIDTH_DEFAULT,
};

float ldki_editor_file_explorer_zoom_get(void)
{
  return s_project_explorer_state.icon_size;
}

void ldki_editor_file_explorer_zoom_set(float zoom)
{
  if (zoom < PROJECT_EXPLORER_MIN_ICON_SIZE)
  {
    zoom = PROJECT_EXPLORER_MIN_ICON_SIZE;
  }
  else if (zoom > 72.0f)
  {
    zoom = 72.0f;
  }

  s_project_explorer_state.icon_size = zoom;
}

float ldki_editor_file_explorer_tree_width_get(void)
{
  return s_project_explorer_state.tree_width;
}

void ldki_editor_file_explorer_tree_width_set(float width)
{
  if (width < PROJECT_EXPLORER_TREE_WIDTH_MIN)
  {
    width = PROJECT_EXPLORER_TREE_WIDTH_MIN;
  }

  s_project_explorer_state.tree_width = width;
}

static const ProjectExplorerFileIcon s_project_explorer_file_icons[] = {
    {"c", LDK_EDITOR_ICON_CODE},
    {"cc", LDK_EDITOR_ICON_CODE},
    {"cpp", LDK_EDITOR_ICON_CODE},
    {"cxx", LDK_EDITOR_ICON_CODE},
    {"h", LDK_EDITOR_ICON_CODE},
    {"hh", LDK_EDITOR_ICON_CODE},
    {"hpp", LDK_EDITOR_ICON_CODE},
    {"hxx", LDK_EDITOR_ICON_CODE},
    {"inl", LDK_EDITOR_ICON_CODE},
    {"glsl", LDK_EDITOR_ICON_CODE},
    {"vert", LDK_EDITOR_ICON_CODE},
    {"frag", LDK_EDITOR_ICON_CODE},
    {"png", LDK_EDITOR_ICON_IMAGE},
    {"jpg", LDK_EDITOR_ICON_IMAGE},
    {"jpeg", LDK_EDITOR_ICON_IMAGE},
    {"bmp", LDK_EDITOR_ICON_IMAGE},
    {"tga", LDK_EDITOR_ICON_IMAGE},
    {"gif", LDK_EDITOR_ICON_IMAGE},
    {"hdr", LDK_EDITOR_ICON_IMAGE},
    {"wav", LDK_EDITOR_ICON_AUDIO_FILE},
    {"ogg", LDK_EDITOR_ICON_AUDIO_FILE},
    {"mp3", LDK_EDITOR_ICON_AUDIO_FILE},
    {"flac", LDK_EDITOR_ICON_AUDIO_FILE},
    {"scene", LDK_EDITOR_ICON_PROJECT},
    {"ldk", LDK_EDITOR_ICON_DATA_OBJECT},
    {"tml", LDK_EDITOR_ICON_DATA_OBJECT},
    {"skybox", LDK_EDITOR_ICON_DATA_OBJECT},
    {"json", LDK_EDITOR_ICON_DATA_OBJECT},
    {"mesh", LDK_EDITOR_ICON_MESH},
    {"obj", LDK_EDITOR_ICON_OBJECT},
    {"fbx", LDK_EDITOR_ICON_OBJECT},
    {"gltf", LDK_EDITOR_ICON_OBJECT},
    {"glb", LDK_EDITOR_ICON_OBJECT},
    {"box", LDK_EDITOR_ICON_BOX},
};

static LDKEditorIcon s_project_explorer_file_icon_get(const XFSPath *path)
{
  XSlice extension;

  if (!path)
  {
    return LDK_EDITOR_ICON_FILE;
  }

  extension = x_fs_path_extension_as_slice(path);
  for (u32 i = 0; i < sizeof(s_project_explorer_file_icons) /
                          sizeof(s_project_explorer_file_icons[0]);
       i++)
  {
    if (x_slice_eq_ci(
            extension, x_slice(s_project_explorer_file_icons[i].extension)))
    {
      return s_project_explorer_file_icons[i].atlas_id;
    }
  }

  return LDK_EDITOR_ICON_FILE;
}

// Thumbnail textures are editor-owned renderer resources. The renderer also
// releases any remaining entries when it terminates.
static void s_project_explorer_thumbnail_release(
    ProjectExplorerThumbnailCache *cache, ProjectExplorerThumbnail *thumbnail)
{
  if (cache->renderer && thumbnail->status == PROJECT_EXPLORER_THUMBNAIL_READY)
  {
    ldk_renderer_texture_destroy(cache->renderer, thumbnail->texture);
  }

  memset(thumbnail, 0, sizeof(*thumbnail));
}

static void s_project_explorer_thumbnail_cache_clear(
    ProjectExplorerThumbnailCache *cache)
{
  for (u32 i = 0; i < PROJECT_EXPLORER_THUMBNAIL_CAPACITY; i++)
  {
    s_project_explorer_thumbnail_release(cache, &cache->entries[i]);
  }
}

static bool s_project_explorer_thumbnail_is_image(const XFSPath *path)
{
  if (!path)
  {
    return false;
  }

  XSlice extension = x_fs_path_extension_as_slice(path);
  static const char *extensions[] = {
      "png", "jpg", "jpeg", "bmp", "tga", "gif", "hdr"};

  for (u32 i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++)
  {
    if (x_slice_eq_ci(extension, x_slice(extensions[i])))
    {
      return true;
    }
  }

  return false;
}

// Area averaging avoids aliasing when a large source is reduced to a small tile.
// RGB is accumulated premultiplied by alpha to avoid transparent edge halos.
static void s_project_explorer_thumbnail_resize(const LDKImageInfo *source,
    u8 *pixels, u32 width, u32 height)
{
  double scale_x = (double)source->width / (double)width;
  double scale_y = (double)source->height / (double)height;
  double area = scale_x * scale_y;

  for (u32 y = 0; y < height; y++)
  {
    double y0 = (double)y * scale_y;
    double y1 = (double)(y + 1) * scale_y;

    for (u32 x = 0; x < width; x++)
    {
      double x0 = (double)x * scale_x;
      double x1 = (double)(x + 1) * scale_x;
      double rgba[4] = {0};

      for (u32 sy = (u32)y0; sy < source->height && (double)sy < y1; sy++)
      {
        double top = y0 > (double)sy ? y0 : (double)sy;
        double bottom = y1 < (double)(sy + 1) ? y1 : (double)(sy + 1);
        double wy = bottom - top;

        for (u32 sx = (u32)x0; sx < source->width && (double)sx < x1; sx++)
        {
          double left = x0 > (double)sx ? x0 : (double)sx;
          double right = x1 < (double)(sx + 1) ? x1 : (double)(sx + 1);
          double weight = (right - left) * wy;
          const u8 *src = source->pixels +
              ((size_t)sy * source->width + sx) * 4u;
          double alpha = (double)src[3] / 255.0;

          rgba[0] += (double)src[0] * alpha * weight;
          rgba[1] += (double)src[1] * alpha * weight;
          rgba[2] += (double)src[2] * alpha * weight;
          rgba[3] += (double)src[3] * weight;
        }
      }

      u8 *dst = pixels + ((size_t)y * width + x) * 4u;
      if (rgba[3] > 0.0)
      {
        double alpha_weight = rgba[3] / 255.0;
        dst[0] = (u8)(rgba[0] / alpha_weight + 0.5);
        dst[1] = (u8)(rgba[1] / alpha_weight + 0.5);
        dst[2] = (u8)(rgba[2] / alpha_weight + 0.5);
      }
      else
      {
        dst[0] = dst[1] = dst[2] = 0;
      }
      dst[3] = (u8)(rgba[3] / area + 0.5);
    }
  }
}

static bool s_project_explorer_thumbnail_create(LDKRenderer *renderer,
    const XFSPath *path, LDKPackage *package,
    ProjectExplorerThumbnail *thumbnail)
{
  LDKImage *image = NULL;
  void *package_data = NULL;

  if (package)
  {
    const LDKPackageEntry *package_entry =
        ldk_package_entry_find(package, x_fs_path_cstr(path));
    if (!package_entry)
    {
      return false;
    }

    u64 package_data_size = ldk_package_entry_get_size(package_entry);
    if (package_data_size == 0 || package_data_size > UINT32_MAX ||
        package_data_size > SIZE_MAX)
    {
      return false;
    }

    package_data = malloc((size_t)package_data_size);
    if (!package_data)
    {
      return false;
    }

    if (!ldk_package_entry_read(
            package, package_entry, package_data, package_data_size))
    {
      free(package_data);
      return false;
    }

    image = ldk_image_create_from_memory(package_data, (u32)package_data_size);
    free(package_data);
  }
  else
  {
    image = ldk_image_load(x_fs_path_cstr(path));
  }
  LDKImageInfo info = {0};
  if (!image)
  {
    return false;
  }

  if (!ldk_image_get_info(image, &info) || !info.pixels ||
      info.width == 0 || info.height == 0 || info.channel_count != 4)
  {
    ldk_image_destroy(image);
    return false;
  }

  u32 width = info.width;
  u32 height = info.height;
  if (width > PROJECT_EXPLORER_THUMBNAIL_SIZE ||
      height > PROJECT_EXPLORER_THUMBNAIL_SIZE)
  {
    if (width >= height)
    {
      height = (u32)((u64)height * PROJECT_EXPLORER_THUMBNAIL_SIZE / width);
      width = PROJECT_EXPLORER_THUMBNAIL_SIZE;
    }
    else
    {
      width = (u32)((u64)width * PROJECT_EXPLORER_THUMBNAIL_SIZE / height);
      height = PROJECT_EXPLORER_THUMBNAIL_SIZE;
    }
  }
  if (width == 0)
  {
    width = 1;
  }
  if (height == 0)
  {
    height = 1;
  }

  u8 *resized = NULL;
  const u8 *pixels = info.pixels;
  if (width != info.width || height != info.height)
  {
    resized = (u8 *)malloc((size_t)width * height * 4u);
    if (!resized)
    {
      ldk_image_destroy(image);
      return false;
    }
    s_project_explorer_thumbnail_resize(&info, resized, width, height);
    pixels = resized;
  }

  LDKRendererTextureOptions options;
  ldk_renderer_texture_options_defaults(&options);
  options.min_filter = LDK_RHI_FILTER_LINEAR;
  options.mag_filter = LDK_RHI_FILTER_LINEAR;

  LDKRendererTextureDesc desc = {0};
  desc.width = width;
  desc.height = height;
  desc.channel_count = 4;
  desc.pixels = pixels;
  desc.byte_count = (u64)width * height * 4u;
  desc.options = &options;

  LDKResourceTexture texture = ldk_renderer_texture_create(renderer, &desc);
  free(resized);
  ldk_image_destroy(image);

  if (!ldk_renderer_texture_is_valid(renderer, texture))
  {
    return false;
  }

  thumbnail->texture = texture;
  thumbnail->width = width;
  thumbnail->height = height;
  return true;
}

static void s_project_explorer_thumbnail_frame_begin(
    ProjectExplorerThumbnailCache *cache, LDKRenderer *renderer)
{
  if (cache->renderer != renderer)
  {
    // The old renderer owns its resources. Never dereference it after a switch.
    memset(cache, 0, sizeof(*cache));
    cache->renderer = renderer;
  }

  cache->frame++;
}

static ProjectExplorerThumbnail *s_project_explorer_thumbnail_request(
    ProjectExplorerThumbnailCache *cache, const ProjectExplorerEntry *entry)
{
  if (!cache->renderer || !s_project_explorer_thumbnail_is_image(&entry->path))
  {
    return NULL;
  }

  ProjectExplorerThumbnail *available = NULL;
  for (u32 i = 0; i < PROJECT_EXPLORER_THUMBNAIL_CAPACITY; i++)
  {
    ProjectExplorerThumbnail *it = &cache->entries[i];
    if (it->status != PROJECT_EXPLORER_THUMBNAIL_EMPTY &&
        it->package == entry->package &&
        x_fs_path_compare(&it->path, &entry->path) == 0)
    {
      if (it->size != entry->size || it->last_modified != entry->last_modified)
      {
        s_project_explorer_thumbnail_release(cache, it);
        break;
      }

      if (it->status == PROJECT_EXPLORER_THUMBNAIL_READY &&
          !ldk_renderer_texture_is_valid(cache->renderer, it->texture))
      {
        it->texture = ldk_renderer_texture_null();
        it->status = PROJECT_EXPLORER_THUMBNAIL_PENDING;
      }

      it->last_used = ++cache->access_counter;
      it->last_seen_frame = cache->frame;
      return it;
    }
  }

  for (u32 i = 0; i < PROJECT_EXPLORER_THUMBNAIL_CAPACITY; i++)
  {
    ProjectExplorerThumbnail *it = &cache->entries[i];
    if (it->status == PROJECT_EXPLORER_THUMBNAIL_EMPTY)
    {
      available = it;
      break;
    }
    if (it->last_seen_frame != cache->frame &&
        (!available || it->last_used < available->last_used))
    {
      available = it;
    }
  }

  if (!available)
  {
    return NULL;
  }

  s_project_explorer_thumbnail_release(cache, available);
  available->path = entry->path;
  available->size = entry->size;
  available->last_modified = entry->last_modified;
  available->package = entry->package;
  available->last_used = ++cache->access_counter;
  available->last_seen_frame = cache->frame;
  available->status = PROJECT_EXPLORER_THUMBNAIL_PENDING;
  return available;
}

static void s_project_explorer_thumbnail_process(
    ProjectExplorerThumbnailCache *cache)
{
  ProjectExplorerThumbnail *pending = NULL;
  for (u32 i = 0; i < PROJECT_EXPLORER_THUMBNAIL_CAPACITY; i++)
  {
    ProjectExplorerThumbnail *it = &cache->entries[i];
    if (it->status == PROJECT_EXPLORER_THUMBNAIL_PENDING &&
        it->last_seen_frame == cache->frame &&
        (!pending || it->last_used < pending->last_used))
    {
      pending = it;
    }
  }

  // At most one image decode and texture upload per explorer update.
  if (pending)
  {
    pending->status = s_project_explorer_thumbnail_create(
        cache->renderer, &pending->path, pending->package, pending)
        ? PROJECT_EXPLORER_THUMBNAIL_READY
        : PROJECT_EXPLORER_THUMBNAIL_FAILED;
  }
}

static LDKUIIcon s_project_explorer_thumbnail_icon(LDKRenderer *renderer,
    const ProjectExplorerThumbnail *thumbnail, LDKUIIcon fallback)
{
  if (!thumbnail || thumbnail->status != PROJECT_EXPLORER_THUMBNAIL_READY)
  {
    return fallback;
  }

  LDKUITextureHandle texture =
      ldk_renderer_texture_ui_handle(renderer, thumbnail->texture);
  if (texture == 0)
  {
    return fallback;
  }

  LDKUIIcon icon = fallback;
  icon.texture = texture;
  icon.uv = (LDKUIRect){0.0f, 0.0f, 1.0f, 1.0f};
  icon.color = 0xffffffffu;
  if (thumbnail->width >= thumbnail->height)
  {
    icon.size.h = fallback.size.h *
        (float)thumbnail->height / (float)thumbnail->width;
  }
  else
  {
    icon.size.w = fallback.size.w *
        (float)thumbnail->width / (float)thumbnail->height;
  }
  return icon;
}

static void s_project_explorer_on_right_click(LDKUIContext *ui,
    ProjectExplorerState *state, const ProjectExplorerEntry *entry,
    bool is_directory, ProjectExplorerSurface surface);
static u32 s_project_explorer_rename_input_draw(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIId id, LDKUIRect rect);
static void s_project_explorer_path_drag_source(
    LDKUIContext *ui, const ProjectExplorerEntry *entry, LDKUIIcon icon);

static bool s_project_explorer_rename_matches(ProjectExplorerState *state,
    const XFSPath *path, ProjectExplorerSurface surface)
{
  return state && path && state->rename_active &&
         state->context_target.surface == surface &&
         x_fs_path_compare(&state->context_target.path, path) == 0;
}

static bool s_project_explorer_initialize(ProjectExplorerState *state)
{
  if (state->expanded_paths == NULL)
  {
    state->expanded_paths =
        x_array_create(sizeof(XFSPath), PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  if (state->package_expanded_paths == NULL)
  {
    state->package_expanded_paths = x_array_create(
        sizeof(ProjectExplorerPackageExpandedPath), PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  if (state->stack == NULL)
  {
    state->stack = x_array_create(
        sizeof(ProjectExplorerNode), PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  if (state->dirs == NULL)
  {
    state->dirs = x_array_create(
        sizeof(ProjectExplorerEntry), PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  if (state->files == NULL)
  {
    state->files = x_array_create(
        sizeof(ProjectExplorerEntry), PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  if (state->package_mounts == NULL)
  {
    state->package_mounts = x_array_create(
        sizeof(ProjectExplorerPackageMount), 8);
  }

  return state->expanded_paths != NULL &&
         state->package_expanded_paths != NULL && state->stack != NULL &&
         state->dirs != NULL && state->files != NULL &&
         state->package_mounts != NULL;
}

static bool s_project_open_default(
    LDKEditorContext *editor, const char *path)
{
  if (path == NULL || path[0] == 0 || !ldk_os_open_default(path))
  {
    ldki_editor_log_error(editor,
        "Failed to open path with the operating system default handler.");
    return false;
  }

  return true;
}

static bool s_project_explorer_extension_list_contains(
    const char *extensions, XSlice extension)
{
  const char *cursor;

  if (extensions == NULL || extension.ptr == NULL || extension.length == 0)
  {
    return false;
  }

  cursor = extensions;
  while (*cursor != 0)
  {
    const char *begin;
    const char *end;
    size_t length;

    while (*cursor != 0 &&
           (*cursor == ',' || *cursor == ';' || isspace((u8)*cursor)))
    {
      ++cursor;
    }

    begin = cursor;
    while (*cursor != 0 && *cursor != ',' && *cursor != ';' &&
           !isspace((u8)*cursor))
    {
      ++cursor;
    }
    end = cursor;

    while (begin < end && (*begin == '*' || *begin == '.'))
    {
      ++begin;
    }
    while (end > begin && isspace((u8)end[-1]))
    {
      --end;
    }

    length = (size_t)(end - begin);
    if (length == extension.length)
    {
      bool matches = true;
      for (size_t i = 0; i < length; ++i)
      {
        if (tolower((u8)begin[i]) != tolower((u8)extension.ptr[i]))
        {
          matches = false;
          break;
        }
      }

      if (matches)
      {
        return true;
      }
    }
  }

  return false;
}

static const LDKEditorFileAssociation *s_project_explorer_association_find(
    const LDKEditorContext *editor, const XFSPath *path)
{
  XSlice extension;

  if (editor == NULL || path == NULL)
  {
    return NULL;
  }

  extension = x_fs_path_extension_as_slice(path);
  if (extension.length == 0)
  {
    return NULL;
  }

  for (u32 i = 0; i < editor->file_association_count; ++i)
  {
    const LDKEditorFileAssociation *association = &editor->file_associations[i];
    if (association->program[0] != 0 &&
        s_project_explorer_extension_list_contains(
            association->extensions, extension))
    {
      return association;
    }
  }

  return NULL;
}

static bool s_project_explorer_arguments_expand(const char *arguments,
    const char *path, char *out, size_t capacity)
{
  static const char placeholder[] = "%file%";
  size_t used = 0;
  bool replaced = false;
  const char *cursor = arguments != NULL ? arguments : "";

  if (path == NULL || out == NULL || capacity == 0)
  {
    return false;
  }

  while (*cursor != 0)
  {
    if (strncmp(cursor, placeholder, sizeof(placeholder) - 1u) == 0)
    {
      int written = snprintf(out + used, capacity - used, "\"%s\"", path);
      if (written < 0 || (size_t)written >= capacity - used)
      {
        return false;
      }
      used += (size_t)written;
      cursor += sizeof(placeholder) - 1u;
      replaced = true;
      continue;
    }

    if (used + 1 >= capacity)
    {
      return false;
    }
    out[used++] = *cursor++;
    out[used] = 0;
  }

  if (!replaced)
  {
    int written = snprintf(out + used, capacity - used, "%s\"%s\"",
        used > 0 ? " " : "", path);
    if (written < 0 || (size_t)written >= capacity - used)
    {
      return false;
    }
  }

  return true;
}

static bool s_project_explorer_association_launch(LDKEditorContext *editor,
    const LDKEditorFileAssociation *association, const XFSPath *path)
{
  char arguments[LDK_EDITOR_FILE_ASSOCIATION_ARGUMENTS_CAPACITY +
                 X_FS_PATH_MAX_LENGTH * 2u + 32u];
  XFSPath working_directory = {0};
  LDKOSProcessDesc process_desc = {0};
  LDKOSProcessResult result;

  if (editor == NULL || association == NULL || path == NULL ||
      association->program[0] == 0 ||
      !s_project_explorer_arguments_expand(
          association->arguments, path->buf, arguments, sizeof(arguments)))
  {
    ldki_editor_log_error(editor, "Invalid file association.");
    return false;
  }

  x_fs_path_dirname(path, &working_directory);
  process_desc.executable = association->program;
  process_desc.arguments = arguments;
  process_desc.working_directory =
      working_directory.length != 0 ? working_directory.buf : ".";
  process_desc.new_console = false;
  result = ldk_os_process_launch(&process_desc);

  if (!result.started)
  {
    char message[256];
    snprintf(message, sizeof(message),
        "Failed to launch file association '%s'.",
        association->name[0] != 0 ? association->name : association->program);
    ldki_editor_log_error(editor, message);
    return false;
  }

  return true;
}

static bool s_project_explorer_file_open(
    LDKEditorContext *editor, const XFSPath *path)
{
  const LDKEditorFileAssociation *association =
      s_project_explorer_association_find(editor, path);
  if (association != NULL)
  {
    return s_project_explorer_association_launch(editor, association, path);
  }

  return s_project_open_default(editor, path->buf);
}

static i32 s_project_explorer_expanded_path_index(
    ProjectExplorerState *state, const XFSPath *path)
{
  for (u32 i = 0; i < x_array_count(state->expanded_paths); i++)
  {
    XFSPath *expanded_path = x_array_get(state->expanded_paths, i);
    if (x_fs_path_compare(expanded_path, path) == 0)
    {
      return (i32)i;
    }
  }

  return -1;
}

static i32 s_project_explorer_package_expanded_path_index(
    ProjectExplorerState *state, LDKPackage *package, const XFSPath *path)
{
  for (u32 i = 0; i < x_array_count(state->package_expanded_paths); ++i)
  {
    ProjectExplorerPackageExpandedPath *expanded =
        x_array_get(state->package_expanded_paths, i);
    if (expanded->package == package &&
        x_fs_path_compare(&expanded->path, path) == 0)
    {
      return (i32)i;
    }
  }

  return -1;
}

static void s_project_explorer_package_expanded_path_set(
    ProjectExplorerState *state, LDKPackage *package, const XFSPath *path,
    bool expanded)
{
  i32 index =
      s_project_explorer_package_expanded_path_index(state, package, path);
  if (expanded)
  {
    if (index < 0)
    {
      ProjectExplorerPackageExpandedPath value = {0};
      value.package = package;
      value.path = *path;
      x_array_add(state->package_expanded_paths, &value);
    }
  }
  else if (index >= 0)
  {
    x_array_delete_at(state->package_expanded_paths, (u32)index);
  }
}

static void s_project_explorer_package_mounts_clear(
    ProjectExplorerState *state)
{
  if (!state || !state->package_mounts)
  {
    return;
  }

  LDKAssetSource *asset_source = ldk_module_get(LDK_MODULE_ASSET_SOURCE);
  for (u32 i = 0; i < x_array_count(state->package_mounts); ++i)
  {
    ProjectExplorerPackageMount *mount = x_array_get(state->package_mounts, i);
    if (asset_source && mount->asset_source_package)
    {
      ldk_asset_source_package_close(asset_source, mount->asset_source_package);
    }
    if (mount->package)
    {
      ldk_package_close(mount->package);
    }
  }
  x_array_clear(state->package_mounts);
  if (state->package_expanded_paths)
  {
    x_array_clear(state->package_expanded_paths);
  }
  state->selected_package = -1;
  memset(&state->selected_package_directory, 0,
      sizeof(state->selected_package_directory));
}

static bool s_project_explorer_root_set(
    ProjectExplorerState *state, const char *root_path)
{
  if (root_path == NULL)
  {
    return false;
  }

  XFSPath root = {0};
  x_fs_path_set(&root, root_path);
  x_fs_path_normalize(&root);

  if (!x_fs_path_is_directory(&root))
  {
    return false;
  }

  if (state->root.length == 0 || x_fs_path_compare(&state->root, &root) != 0)
  {
    s_project_explorer_thumbnail_cache_clear(&state->thumbnails);
    s_project_explorer_package_mounts_clear(state);
    state->root = root;
    state->selected_directory = root;
    memset(&state->selected_file, 0, sizeof(state->selected_file));
    memset(&state->context_target, 0, sizeof(state->context_target));
    memset(&state->rename_input_rect, 0, sizeof(state->rename_input_rect));
    state->rename_input_id = 0;
    state->rename_buffer[0] = 0;
    state->rename_active = false;
    state->rename_focus_requested = false;
    state->rename_had_focus = false;
    x_array_clear(state->expanded_paths);
    state->last_click_ticks = 0;
    state->last_click_package = NULL;
    state->root_expanded = true;
    state->selected_package = -1;
    memset(&state->selected_package_directory, 0,
        sizeof(state->selected_package_directory));
  }

  return true;
}

static void s_project_explorer_directory_select(
    ProjectExplorerState *state, const XFSPath *path, bool expand)
{
  state->selected_directory = *path;
  state->selected_package = -1;
  memset(&state->selected_package_directory, 0, sizeof(state->selected_package_directory));
  memset(&state->selected_file, 0, sizeof(state->selected_file));
  state->last_click_ticks = 0;
  state->last_click_package = NULL;

  if (!expand)
  {
    return;
  }

  if (x_fs_path_compare(&state->root, path) == 0)
  {
    state->root_expanded = true;
    return;
  }

  if (s_project_explorer_expanded_path_index(state, path) < 0)
  {
    x_array_add(state->expanded_paths, (XFSPath *)path);
  }
}

static void s_project_explorer_entry_insert_sorted(
    XArray *entries, const ProjectExplorerEntry *entry)
{
  u32 insert_index = x_array_count(entries);

  for (u32 i = 0; i < x_array_count(entries); i++)
  {
    ProjectExplorerEntry *it = x_array_get(entries, i);
    if (strcmp(it->name.buf, entry->name.buf) > 0)
    {
      insert_index = i;
      break;
    }
  }

  x_array_insert(entries, (XArray *)entry, insert_index);
}

static void s_project_explorer_directory_read(
    const XFSPath *path, XArray *dirs, XArray *files)
{
  if (dirs != NULL)
  {
    x_array_clear(dirs);
  }

  if (files != NULL)
  {
    x_array_clear(files);
  }

  XFSDireEntry fs_entry = {0};
  XFSDireHandle *dir = x_fs_find_first_file(x_fs_path_cstr(path), &fs_entry);

  while (dir != NULL)
  {
    bool special_entry =
        strcmp(fs_entry.name, ".") == 0 || strcmp(fs_entry.name, "..") == 0;
    XArray *target = fs_entry.is_directory ? dirs : files;

    if (!special_entry && target != NULL)
    {
      ProjectExplorerEntry entry = {0};
      entry.path = *path;
      x_fs_path_join(&entry.path, fs_entry.name);
      x_smallstr_from_cstr(&entry.name, fs_entry.name);
      entry.size = fs_entry.size;
      entry.last_modified = fs_entry.last_modified;
      s_project_explorer_entry_insert_sorted(target, &entry);
    }

    if (!x_fs_find_next_file(dir, &fs_entry))
    {
      break;
    }
  }

  if (dir != NULL)
  {
    x_fs_find_close(dir);
  }
}

static bool s_project_explorer_package_directory_read(
    ProjectExplorerState *state, u32 package_index, const char *directory,
    XArray *dirs, XArray *files)
{
  if (!state || !state->package_mounts ||
      package_index >= x_array_count(state->package_mounts))
  {
    return false;
  }

  if (dirs) x_array_clear(dirs);
  if (files) x_array_clear(files);

  ProjectExplorerPackageMount *mount =
      x_array_get(state->package_mounts, package_index);
  size_t directory_length = directory ? strlen(directory) : 0;

  for (u32 i = 0; i < ldk_package_entry_count(mount->package); ++i)
  {
    const LDKPackageEntry *package_entry =
        ldk_package_entry_at(mount->package, i);
    const char *entry_path = ldk_package_entry_get_path(package_entry);
    if (!entry_path) continue;

    const char *relative = entry_path;
    if (directory_length)
    {
      if (strncmp(entry_path, directory, directory_length) != 0 ||
          entry_path[directory_length] != '/')
      {
        continue;
      }
      relative = entry_path + directory_length + 1;
    }

    if (!relative[0]) continue;
    const char *slash = strchr(relative, '/');
    ProjectExplorerEntry entry = {0};
    size_t name_length = slash ? (size_t)(slash - relative) : strlen(relative);
    if (name_length == 0 || name_length > X_SMALLSTR_MAX_LENGTH) continue;

    char name[X_SMALLSTR_MAX_LENGTH + 1];
    memcpy(name, relative, name_length);
    name[name_length] = 0;
    x_smallstr_from_cstr(&entry.name, name);
    entry.package = mount->package;
    entry.package_backed = true;
    entry.is_directory = slash != NULL;

    char logical[LDK_ASSET_PATH_MAX_LENGTH + 1];
    int written = directory_length
        ? snprintf(logical, sizeof(logical), "%s/%s", directory, name)
        : snprintf(logical, sizeof(logical), "%s", name);
    if (written <= 0 || (size_t)written >= sizeof(logical)) continue;
    x_fs_path_set(&entry.path, logical);

    if (entry.is_directory)
    {
      bool exists = false;
      if (dirs)
      {
        for (u32 d = 0; d < x_array_count(dirs); ++d)
        {
          ProjectExplorerEntry *existing = x_array_get(dirs, d);
          if (strcmp(existing->name.buf, entry.name.buf) == 0)
          {
            exists = true;
            break;
          }
        }
        if (!exists) s_project_explorer_entry_insert_sorted(dirs, &entry);
      }
    }
    else if (files && ldk_asset_path_set(&entry.asset_path, entry_path))
    {
      entry.size = (size_t)ldk_package_entry_get_size(package_entry);
      s_project_explorer_entry_insert_sorted(files, &entry);
    }
  }
  return true;
}


void ldki_editor_file_explorer_package_mounts_clear(void)
{
  s_project_explorer_package_mounts_clear(&s_project_explorer_state);
}

void ldki_editor_file_explorer_focus_runtree(LDKEditorContext *editor)
{
  ProjectExplorerState *state = &s_project_explorer_state;

  if (editor == NULL || !editor->project.loaded ||
      !s_project_explorer_initialize(state) ||
      !s_project_explorer_root_set(state, editor->project.run_root_path.buf))
  {
    return;
  }

  s_project_explorer_directory_select(state, &state->root, true);
  state->tree_scroll = (LDKUIPoint){0};
  state->file_scroll = (LDKUIPoint){0};
}

bool ldki_editor_file_explorer_package_mount(
    LDKEditorContext *editor, const XFSPath *package_path)
{
  ProjectExplorerState *state = &s_project_explorer_state;
  if (!editor || !package_path || !x_fs_path_is_file(package_path) ||
      !s_project_explorer_initialize(state))
  {
    return false;
  }

  if (editor->project.loaded &&
      !s_project_explorer_root_set(state, editor->project.run_root_path.buf))
  {
    return false;
  }

  XFSPath normalized = *package_path;
  x_fs_path_normalize(&normalized);
  for (u32 i = 0; i < x_array_count(state->package_mounts); ++i)
  {
    ProjectExplorerPackageMount *existing =
        x_array_get(state->package_mounts, i);
    if (x_fs_path_compare(&existing->path, &normalized) == 0)
    {
      state->selected_package = (i32)i;
      memset(&state->selected_package_directory, 0,
          sizeof(state->selected_package_directory));
      return true;
    }
  }

  LDKPackage *package = ldk_package_open(normalized.buf);
  if (!package) return false;

  LDKAssetSource *asset_source = ldk_module_get(LDK_MODULE_ASSET_SOURCE);
  LDKAssetSourcePackage *source_package = asset_source
      ? ldk_asset_source_package_open(asset_source, normalized.buf) : NULL;
  if (asset_source && !source_package)
  {
    ldk_package_close(package);
    return false;
  }

  ProjectExplorerPackageMount mount = {0};
  mount.path = normalized;
  mount.package = package;
  mount.asset_source_package = source_package;
  x_fs_path_basename(&normalized, &mount.name);
  if (x_array_add(state->package_mounts, &mount) != 0)
  {
    if (source_package) ldk_asset_source_package_close(asset_source, source_package);
    ldk_package_close(package);
    return false;
  }

  state->selected_package = (i32)x_array_count(state->package_mounts) - 1;
  memset(&state->selected_package_directory, 0,
      sizeof(state->selected_package_directory));
  memset(&state->selected_file, 0, sizeof(state->selected_file));
  return true;
}

bool ldki_editor_file_explorer_reveal_asset(
    LDKEditorContext *editor, const char *asset_path)
{
  ProjectExplorerState *state = &s_project_explorer_state;
  LDKAssetSource *source = ldk_module_get(LDK_MODULE_ASSET_SOURCE);
  LDKAssetSourceFile file;
  XFSPath path = {0};
  XFSPath directory = {0};
  LDKPackage *package = NULL;

  if (!editor || !editor->project.loaded ||
      !s_project_explorer_initialize(state) ||
      !s_project_explorer_root_set(state, editor->project.run_root_path.buf) ||
      !ldk_asset_source_find(source, asset_path, &file))
  {
    return false;
  }

  if (file.origin == LDK_ASSET_SOURCE_ORIGIN_PACKAGE)
  {
    const XFSPath *package_path = ldk_asset_source_package_path_get(
        source, file.location.package.package);
    if (!package_path ||
        !ldki_editor_file_explorer_package_mount(editor, package_path))
    {
      return false;
    }
    ProjectExplorerPackageMount *mount =
        x_array_get(state->package_mounts, (u32)state->selected_package);
    package = mount->package;
    if (!x_fs_path_set(&path,
            ldk_package_entry_get_path(file.location.package.entry))) return false;
    x_fs_path_dirname(&path, &directory);
    x_smallstr_from_cstr(&state->selected_package_directory, directory.buf);
    XFSPath ancestor = directory;
    for (;;)
    {
      s_project_explorer_package_expanded_path_set(state, package, &ancestor, true);
      if (ancestor.length == 0) break;
      XFSPath parent = {0};
      x_fs_path_dirname(&ancestor, &parent);
      ancestor = parent;
    }
  }
  else
  {
    path = file.location.filesystem_path;
    x_fs_path_dirname(&path, &directory);
    s_project_explorer_directory_select(state, &directory, true);
    XFSPath ancestor = directory;
    while (x_fs_path_compare(&ancestor, &state->root) != 0)
    {
      if (s_project_explorer_expanded_path_index(state, &ancestor) < 0)
        x_array_add(state->expanded_paths, &ancestor);
      XFSPath parent = {0};
      if (!x_fs_path_dirname(&ancestor, &parent) ||
          x_fs_path_compare(&ancestor, &parent) == 0) break;
      ancestor = parent;
    }
    state->root_expanded = true;
  }

  state->selected_file = path;
  state->last_click_package = package;
  state->last_click_ticks = 0;
  state->file_scroll = (LDKUIPoint){0};
  state->reveal_scroll_pending = true;
  return ldki_editor_window_activate(LDK_EDITOR_WINDOW_PROJECT_EXPLORER);
}

static bool s_project_explorer_rect_visible(LDKUIRect rect, LDKUIRect clip)
{
  return rect.w > 0.0f && rect.h > 0.0f &&
         rect.x < clip.x + clip.w && rect.x + rect.w > clip.x &&
         rect.y < clip.y + clip.h && rect.y + rect.h > clip.y;
}

static bool s_project_explorer_rect_button_down(
    LDKUIContext *ui, LDKUIRect rect, LDKMouseButton button)
{
  LDKMouseState *mouse;
  LDKPoint cursor;

  if (!ui || !ui->mouse)
  {
    return false;
  }

  mouse = (LDKMouseState *)ui->mouse;
  cursor = ldk_os_mouse_cursor(mouse);
  return ldk_os_mouse_button_down(mouse, button) &&
         ldk_rectf_contains(&rect, (float)cursor.x, (float)cursor.y);
}

static bool s_project_explorer_icon_valid(LDKUIIcon icon)
{
  return icon.texture != 0 && icon.uv.w > 0.0f && icon.uv.h > 0.0f &&
         icon.size.w > 0.0f && icon.size.h > 0.0f;
}

static LDKUIRect s_project_explorer_tree_rename_rect(LDKUIContext *ui,
    LDKUIRect row_rect, const ProjectExplorerNode *node, LDKUIIcon folder_icon,
    bool expanded)
{
  LDKUIIcon chevron =
      ui->theme.icons[expanded ? LDK_UI_THEME_ICON_TREE_NODE_EXPANDED
                               : LDK_UI_THEME_ICON_TREE_NODE_COLLAPSED];
  float chevron_width = s_project_explorer_icon_valid(chevron)
                            ? chevron.size.w
                            : LDK_UI_TREE_NODE_CHEVRON_WIDTH;
  float input_x = row_rect.x +
                  (float)node->depth * LDK_UI_TREE_NODE_INDENT_WIDTH +
                  chevron_width + LDK_UI_DEFAULT_SPACING;

  if (s_project_explorer_icon_valid(folder_icon))
  {
    input_x += folder_icon.size.w + LDK_UI_DEFAULT_SPACING;
  }

  LDKUIRect input_rect = row_rect;
  input_rect.x = input_x;
  input_rect.w = row_rect.x + row_rect.w - input_x;
  if (input_rect.w < 0.0f)
  {
    input_rect.w = 0.0f;
  }

  return input_rect;
}

static bool s_project_explorer_tree_node(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui,
    const ProjectExplorerNode *node, LDKUIIcon folder_icon)
{
  i32 expanded_index =
      node->root ? -1
                 : s_project_explorer_expanded_path_index(state, &node->path);
  bool was_expanded = node->root ? state->root_expanded : expanded_index >= 0;
  u32 flags = 0;

  if (x_fs_path_compare(&state->selected_directory, &node->path) == 0)
  {
    flags |= LDK_UI_TREE_NODE_SELECTED;
  }

  bool renaming = s_project_explorer_rename_matches(
      state, &node->path, PROJECT_EXPLORER_SURFACE_TREE);
  if (renaming)
  {
    ldk_ui_set_next_width(ui, ldk_ui_fill());
  }

  u32 result = ldk_ui_tree_node_ex(ui, renaming ? "" : node->name.buf,
      folder_icon, was_expanded, node->depth, flags);
  LDKUIRect row_rect = ldk_ui_last_bounding_rect(ui);
  LDKUIId node_id = ui->last_id;
  bool expanded = was_expanded;

  if (!renaming)
  {
    ProjectExplorerEntry drag_entry = {0};
    drag_entry.path = node->path;
    drag_entry.is_directory = true;
    s_project_explorer_path_drag_source(ui, &drag_entry, folder_icon);
  }

  if (renaming)
  {
    LDKUIRect input_rect = s_project_explorer_tree_rename_rect(
        ui, row_rect, node, folder_icon, was_expanded);
    s_project_explorer_rename_input_draw(editor, state, ui,
        node_id ^ PROJECT_EXPLORER_TREE_RENAME_INPUT_ID, input_rect);
  }
  else if (s_project_explorer_rect_button_down(
               ui, row_rect, LDK_MOUSE_BUTTON_RIGHT))
  {
    ProjectExplorerEntry entry = {0};
    entry.path = node->path;
    entry.name = node->name;
    s_project_explorer_on_right_click(
        ui, state, &entry, true, PROJECT_EXPLORER_SURFACE_TREE);
  }

  if (renaming)
  {
    return was_expanded;
  }

  if (result & LDK_UI_TREE_NODE_RESULT_CLICKED)
  {
    s_project_explorer_directory_select(state, &node->path, false);
  }

  if (result & LDK_UI_TREE_NODE_RESULT_TOGGLED)
  {
    expanded = !was_expanded;
  }

  if (expanded == was_expanded)
  {
    return expanded;
  }

  s_project_explorer_directory_select(state, &node->path, false);

  if (node->root)
  {
    state->root_expanded = expanded;
  }
  else if (expanded)
  {
    x_array_add(state->expanded_paths, (ProjectExplorerNode *)&node->path);
  }
  else
  {
    x_array_delete_at(state->expanded_paths, (u32)expanded_index);
  }

  return expanded;
}

static bool s_project_explorer_package_tree_node(
    ProjectExplorerState *state, LDKUIContext *ui,
    const ProjectExplorerNode *node, LDKUIIcon icon)
{
  ProjectExplorerPackageMount *mount =
      x_array_get(state->package_mounts, node->package_index);
  bool was_expanded = s_project_explorer_package_expanded_path_index(
          state, mount->package, &node->path) >= 0;
  u32 flags = 0;

  if (state->selected_package == (i32)node->package_index &&
      strcmp(state->selected_package_directory.buf, node->path.buf) == 0)
  {
    flags |= LDK_UI_TREE_NODE_SELECTED;
  }

  u32 result = ldk_ui_tree_node_ex(
      ui, node->name.buf, icon, was_expanded, node->depth, flags);
  bool expanded = was_expanded;

  if (result & LDK_UI_TREE_NODE_RESULT_CLICKED)
  {
    state->selected_package = (i32)node->package_index;
    x_smallstr_from_cstr(&state->selected_package_directory, node->path.buf);
    memset(&state->selected_file, 0, sizeof(state->selected_file));
  }

  if (result & LDK_UI_TREE_NODE_RESULT_TOGGLED)
  {
    expanded = !was_expanded;
  }

  if (expanded != was_expanded)
  {
    s_project_explorer_package_expanded_path_set(
        state, mount->package, &node->path, expanded);
  }

  return expanded;
}

static void s_project_explorer_tree_draw(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIIcon folder_icon)
{
  state->tree_scroll = ldk_ui_begin_scrollview(
      ui, state->tree_scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  x_array_clear(state->stack);

  ProjectExplorerNode root_node = {0};
  root_node.path = state->root;
  x_fs_path_basename(&state->root, &root_node.name);
  if (root_node.name.length == 0)
  {
    x_smallstr_from_cstr(&root_node.name, x_fs_path_cstr(&state->root));
  }
  root_node.root = true;
  x_array_add(state->stack, &root_node);

  while (x_array_count(state->stack) > 0)
  {
    u32 stack_index = x_array_count(state->stack) - 1;
    ProjectExplorerNode *node_ptr = x_array_get(state->stack, stack_index);
    ProjectExplorerNode node = *node_ptr;
    x_array_delete_at(state->stack, stack_index);

    if (!s_project_explorer_tree_node(editor, state, ui, &node, folder_icon))
    {
      continue;
    }

    s_project_explorer_directory_read(&node.path, state->dirs, NULL);

    for (u32 i = x_array_count(state->dirs); i > 0; i--)
    {
      ProjectExplorerEntry *entry = x_array_get(state->dirs, i - 1);
      ProjectExplorerNode child = {0};
      child.path = entry->path;
      child.name = entry->name;
      child.depth = node.depth + 1;
      x_array_add(state->stack, &child);
    }
  }

  for (u32 i = 0; i < x_array_count(state->package_mounts); ++i)
  {
    ProjectExplorerPackageMount *mount = x_array_get(state->package_mounts, i);
    LDKUIIcon package_icon = folder_icon;
    package_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_BOX];

    x_array_clear(state->stack);
    ProjectExplorerNode package_root = {0};
    package_root.name = mount->name;
    package_root.package_index = i;
    package_root.package_backed = true;
    package_root.root = true;
    x_array_add(state->stack, &package_root);

    while (x_array_count(state->stack) > 0)
    {
      u32 stack_index = x_array_count(state->stack) - 1;
      ProjectExplorerNode *node_ptr = x_array_get(state->stack, stack_index);
      ProjectExplorerNode node = *node_ptr;
      x_array_delete_at(state->stack, stack_index);

      LDKUIIcon node_icon = node.root ? package_icon : folder_icon;
      if (!s_project_explorer_package_tree_node(
              state, ui, &node, node_icon))
      {
        continue;
      }

      s_project_explorer_package_directory_read(
          state, i, node.path.buf, state->dirs, NULL);
      for (u32 d = x_array_count(state->dirs); d > 0; --d)
      {
        ProjectExplorerEntry *entry = x_array_get(state->dirs, d - 1);
        ProjectExplorerNode child = {0};
        child.path = entry->path;
        child.name = entry->name;
        child.depth = node.depth + 1;
        child.package_index = i;
        child.package_backed = true;
        x_array_add(state->stack, &child);
      }
    }
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
}

static ProjectExplorerEntry *s_project_explorer_entry_get(
    ProjectExplorerState *state, u32 index, bool *is_directory)
{
  u32 directory_count = x_array_count(state->dirs);
  *is_directory = index < directory_count;

  if (*is_directory)
  {
    return x_array_get(state->dirs, index);
  }

  return x_array_get(state->files, index - directory_count);
}

static void s_project_explorer_on_file_double_click(
    LDKEditorContext *editor, const ProjectExplorerEntry *entry)
{
  if (!editor || !entry)
  {
    return;
  }

  XSlice extension = x_fs_path_extension_as_slice(&entry->path);
  if (x_slice_eq_ci(extension, x_slice("box")))
  {
    if (!ldki_editor_file_explorer_package_mount(editor, &entry->path))
    {
      ldki_editor_log_error(editor, "Failed to mount package.");
    }
  }
  else if (ldki_editor_scene_path_is_scene(&entry->path))
  {
    ldki_editor_scene_load(editor, &entry->path);
  }
  else
  {
    s_project_explorer_file_open(editor, &entry->path);
  }
}

static bool s_project_explorer_entry_double_click(
    ProjectExplorerState *state, const ProjectExplorerEntry *entry)
{
  u64 now;
  bool same_entry;
  bool double_click;

  if (state == NULL || entry == NULL)
  {
    return false;
  }

  now = ldk_os_time_ticks_get();
  same_entry = state->selected_file.length != 0 &&
               state->last_click_package == entry->package &&
               x_fs_path_compare(&state->selected_file, &entry->path) == 0;
  double_click =
      same_entry && state->last_click_ticks != 0 &&
      ldk_os_time_ticks_interval_get_seconds(state->last_click_ticks, now) <=
          PROJECT_EXPLORER_DOUBLE_CLICK_SECONDS;

  state->selected_file = entry->path;
  state->last_click_package = entry->package;
  state->last_click_ticks = double_click ? 0 : now;
  return double_click;
}

static void s_project_explorer_entry_activate(LDKEditorContext *editor,
    ProjectExplorerState *state, const ProjectExplorerEntry *entry,
    bool is_directory)
{
  bool activate = false;

  if (editor == NULL || state == NULL || entry == NULL)
  {
    return;
  }

  if (is_directory)
  {
    activate = editor->file_explorer_open_folders_single_click ||
               s_project_explorer_entry_double_click(state, entry);
    if (!activate)
    {
      return;
    }

    if (entry->package_backed)
    {
      x_smallstr_from_cstr(
          &state->selected_package_directory, entry->path.buf);
      memset(&state->selected_file, 0, sizeof(state->selected_file));
      state->last_click_ticks = 0;
      state->last_click_package = NULL;
    }
    else
    {
      s_project_explorer_directory_select(state, &entry->path, true);
    }
    return;
  }

  if (s_project_explorer_entry_double_click(state, entry))
  {
    s_project_explorer_on_file_double_click(editor, entry);
  }
}

static void s_project_explorer_on_right_click(LDKUIContext *ui,
    ProjectExplorerState *state, const ProjectExplorerEntry *entry,
    bool is_directory, ProjectExplorerSurface surface)
{
  LDKPoint cursor;
  LDKUIPoint position;

  if (!ui || !ui->mouse || !state || !entry || state->rename_active)
  {
    return;
  }

  state->context_target.path = entry->path;
  state->context_target.is_directory = is_directory;
  state->context_target.surface = surface;
  state->last_click_ticks = 0;
  state->last_click_package = NULL;

  cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
  position.x = (float)cursor.x;
  position.y = (float)cursor.y;

  ldk_ui_close_all_popups(ui);
  ldk_ui_open_popup_at(ui, PROJECT_EXPLORER_CONTEXT_POPUP_ID, position);
}

static bool s_project_explorer_path_name_valid(const char *name)
{
  if (!name || name[0] == 0 || strcmp(name, ".") == 0 ||
      strcmp(name, "..") == 0)
  {
    return false;
  }

  for (const char *cursor = name; *cursor != 0; cursor++)
  {
    if (*cursor == '/' || *cursor == '\\' || *cursor == ':' || *cursor == '*' ||
        *cursor == '?' || *cursor == '"' || *cursor == '<' || *cursor == '>' ||
        *cursor == '|')
    {
      return false;
    }
  }

  return true;
}

static bool s_project_explorer_duplicate_path_get(
    const XFSPath *path, bool is_directory, u32 copy_index, XFSPath *out_path)
{
  XFSPath directory = {0};
  XSlice basename;
  XSlice extension;
  size_t stem_length;
  char name[X_SMALLSTR_MAX_LENGTH + 1];
  int length;

  if (!path || !out_path || copy_index == 0 ||
      x_fs_path_dirname(path, &directory) == 0)
  {
    return false;
  }

  basename = x_fs_path_basename_as_slice(path);
  extension = is_directory ? x_slice("") : x_fs_path_extension_as_slice(path);
  stem_length = basename.length;

  if (extension.length > 0 && basename.length > extension.length + 1)
  {
    stem_length -= extension.length + 1;
  }
  else
  {
    extension.length = 0;
  }

  if (copy_index == 1)
  {
    length = snprintf(name, sizeof(name), "%.*s copy%s%.*s", (int)stem_length,
        basename.ptr, extension.length > 0 ? "." : "", (int)extension.length,
        extension.ptr);
  }
  else
  {
    length = snprintf(name, sizeof(name), "%.*s copy %u%s%.*s",
        (int)stem_length, basename.ptr, copy_index,
        extension.length > 0 ? "." : "", (int)extension.length, extension.ptr);
  }

  if (length <= 0 || (size_t)length >= sizeof(name))
  {
    return false;
  }

  return x_fs_path(out_path, x_fs_path_cstr(&directory), name);
}

static bool s_project_explorer_path_duplicate(
    const XFSPath *path, bool is_directory, XFSPath *out_path)
{
  XFSPath destination = {0};

  if (!path || !out_path || !x_fs_path_exists(path))
  {
    return false;
  }

  for (u32 copy_index = 1; copy_index < 1000; copy_index++)
  {
    if (!s_project_explorer_duplicate_path_get(
            path, is_directory, copy_index, &destination))
    {
      return false;
    }

    if (!x_fs_path_exists(&destination))
    {
      bool copied = is_directory
                        ? x_fs_directory_copy(path->buf, destination.buf)
                        : x_fs_file_copy(path->buf, destination.buf);

      if (copied)
      {
        *out_path = destination;
      }
      return copied;
    }
  }

  return false;
}

static bool s_project_explorer_path_rename(const XFSPath *path,
    bool is_directory, const char *new_name, XFSPath *out_path)
{
  XFSPath directory = {0};
  XFSPath destination = {0};

  if (!path || !out_path || !s_project_explorer_path_name_valid(new_name) ||
      x_fs_path_dirname(path, &directory) == 0 ||
      !x_fs_path(&destination, directory.buf, new_name))
  {
    return false;
  }

  if (x_fs_path_compare(path, &destination) == 0)
  {
    *out_path = *path;
    return true;
  }

  if (x_fs_path_exists(&destination))
  {
    return false;
  }

  bool renamed = is_directory
                     ? x_fs_directory_rename(path->buf, destination.buf)
                     : x_fs_file_rename(path->buf, destination.buf);
  if (!renamed)
  {
    return false;
  }

  *out_path = destination;
  return true;
}

static void s_project_explorer_rename_end(ProjectExplorerState *state)
{
  if (!state)
  {
    return;
  }

  state->rename_active = false;
  state->rename_focus_requested = false;
  state->rename_had_focus = false;  state->rename_input_id = 0;
  memset(&state->rename_input_rect, 0, sizeof(state->rename_input_rect));
  state->rename_buffer[0] = 0;
}

static void s_project_explorer_current_scene_renamed(
    LDKEditorContext *editor, const XFSPath *old_path,
    const XFSPath *new_path)
{
  XFSPath current = {0};
  XFSPath relative = {0};

  if (editor == NULL || old_path == NULL || new_path == NULL ||
      !editor->project.loaded || editor->current_scene_path.length == 0)
  {
    return;
  }

  x_fs_path(&current, editor->project.run_root_path.buf,
      editor->current_scene_path.buf);
  x_fs_path_normalize(&current);
  if (x_fs_path_compare(&current, old_path) != 0)
  {
    return;
  }

  if (x_fs_path_relative_to(
          &editor->project.run_root_path, new_path, &relative) > 0)
  {
    editor->current_scene_path = relative;
  }
}

static bool s_project_explorer_rename_commit(
    LDKEditorContext *editor, ProjectExplorerState *state)
{
  XFSPath old_path;
  XFSPath renamed = {0};

  if (!editor || !state || !state->rename_active)
  {
    return false;
  }

  old_path = state->context_target.path;
  if (!s_project_explorer_path_rename(&old_path,
          state->context_target.is_directory, state->rename_buffer, &renamed))
  {
    ldki_editor_log_error(editor, "Failed to rename path.");
    state->rename_focus_requested = true;
    state->rename_had_focus = false;
    return false;
  }

  state->context_target.path = renamed;

  if (!state->context_target.is_directory)
  {
    s_project_explorer_current_scene_renamed(editor, &old_path, &renamed);
  }

  if (state->context_target.is_directory)
  {
    s_project_explorer_directory_select(state, &renamed, false);
    x_array_clear(state->expanded_paths);
  }
  else if (x_fs_path_compare(&state->selected_file, &old_path) == 0)
  {
    state->selected_file = renamed;
  }

  s_project_explorer_rename_end(state);
  return true;
}

static void s_project_explorer_rename_begin(ProjectExplorerState *state)
{
  XFSPath basename = {0};

  if (!state || x_fs_path_basename(&state->context_target.path, &basename) == 0)
  {
    return;
  }

  snprintf(
      state->rename_buffer, sizeof(state->rename_buffer), "%s", basename.buf);
  state->rename_active = true;
  state->rename_focus_requested = true;
  state->rename_had_focus = false;
  state->rename_input_id = 0;
  memset(&state->rename_input_rect, 0, sizeof(state->rename_input_rect));
}

static void s_project_explorer_rename_input_prepare(
    ProjectExplorerState *state, LDKUIContext *ui)
{
  if (state->rename_focus_requested)
  {
    ldk_ui_set_next_focus(ui);
    state->rename_focus_requested = false;
  }
}

static u32 s_project_explorer_rename_input_finish(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIId id, LDKUIRect rect,
    u32 result)
{
  state->rename_input_id = id;
  state->rename_input_rect = rect;

  if (ui->focused_id == id)
  {
    state->rename_had_focus = true;
  }

  if ((result & LDK_UI_INPUT_BOX_CANCELED) != 0)
  {
    s_project_explorer_rename_end(state);
  }
  else if ((result & LDK_UI_INPUT_BOX_COMMITTED) != 0)
  {
    s_project_explorer_rename_commit(editor, state);
  }

  return result;
}

static u32 s_project_explorer_rename_input_draw(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIId id, LDKUIRect rect)
{
  if (!editor || !state || !ui || !state->rename_active)
  {
    return LDK_UI_INPUT_BOX_NONE;
  }

  s_project_explorer_rename_input_prepare(state, ui);
  u32 result = ldk_ui_widget_input_label(
      ui, id, state->rename_buffer, (u32)sizeof(state->rename_buffer), rect);
  return s_project_explorer_rename_input_finish(
      editor, state, ui, id, rect, result);
}

static u32 s_project_explorer_rename_input_layout_draw(
    LDKEditorContext *editor, ProjectExplorerState *state, LDKUIContext *ui)
{
  if (!editor || !state || !ui || !state->rename_active)
  {
    return LDK_UI_INPUT_BOX_NONE;
  }

  s_project_explorer_rename_input_prepare(state, ui);
  u32 result = ldk_ui_input_label(
      ui, state->rename_buffer, (u32)sizeof(state->rename_buffer));
  return s_project_explorer_rename_input_finish(
      editor, state, ui, ui->last_id, ldk_ui_last_bounding_rect(ui), result);
}

static void s_project_explorer_rename_before_draw(
    LDKEditorContext *editor, ProjectExplorerState *state, LDKUIContext *ui)
{
  if (!editor || !state || !ui || !state->rename_active ||
      !state->rename_had_focus)
  {
    return;
  }

  bool focus_lost = ui->focused_id != state->rename_input_id;
  bool clicked_outside = false;

  if (ui->mouse && ldk_os_mouse_button_down(
                       (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
    clicked_outside = !ldk_rectf_contains(
        &state->rename_input_rect, (float)cursor.x, (float)cursor.y);
  }

  if (focus_lost || clicked_outside)
  {
    s_project_explorer_rename_commit(editor, state);
  }
}

static bool s_project_explorer_unique_child_path(const XFSPath *directory,
    const char *base_name, const char *extension, XFSPath *out_path)
{
  char name[X_SMALLSTR_MAX_LENGTH + 32];

  if (directory == NULL || base_name == NULL || extension == NULL ||
      out_path == NULL)
  {
    return false;
  }

  for (u32 i = 0; i < 10000; ++i)
  {
    int written =
        i == 0
            ? snprintf(name, sizeof(name), "%s%s", base_name, extension)
            : snprintf(name, sizeof(name), "%s %u%s", base_name, i + 1u,
                  extension);
    if (written <= 0 || (size_t)written >= sizeof(name))
    {
      return false;
    }

    x_fs_path(out_path, directory->buf, name);
    x_fs_path_normalize(out_path);
    if (!x_fs_path_exists(out_path))
    {
      return true;
    }
  }

  return false;
}

static bool s_project_explorer_create_directory(
    LDKEditorContext *editor, ProjectExplorerState *state)
{
  XFSPath parent;
  XFSPath created = {0};
  (void)editor;

  if (state == NULL || !state->context_target.is_directory)
  {
    return false;
  }

  parent = state->context_target.path;
  if (!s_project_explorer_unique_child_path(
          &parent, "New Folder", "", &created) ||
      !x_fs_directory_create(created.buf))
  {
    return false;
  }

  s_project_explorer_directory_select(state, &parent, true);
  state->context_target.path = created;
  state->context_target.is_directory = true;
  state->context_target.surface = PROJECT_EXPLORER_SURFACE_FILES;
  s_project_explorer_rename_begin(state);
  return true;
}

static bool s_project_explorer_create_scene(
    LDKEditorContext *editor, ProjectExplorerState *state)
{
  XFSPath parent;
  XFSPath created = {0};

  if (editor == NULL || state == NULL || !state->context_target.is_directory)
  {
    return false;
  }

  parent = state->context_target.path;
  if (!s_project_explorer_unique_child_path(
          &parent, "New Scene", ".scene", &created) ||
      !ldki_editor_scene_new_at_path(editor, &created))
  {
    return false;
  }

  s_project_explorer_directory_select(state, &parent, true);
  state->selected_file = created;
  state->context_target.path = created;
  state->context_target.is_directory = false;
  state->context_target.surface = PROJECT_EXPLORER_SURFACE_FILES;
  s_project_explorer_rename_begin(state);
  return true;
}

static bool s_project_explorer_create_material(
    LDKEditorContext *editor, ProjectExplorerState *state)
{
  XFSPath parent;
  XFSPath created = {0};
  LDKMaterialDesc material = {0};
  LDKMaterialIOContext context = {0};
  LDKMaterialIOResult result = {0};
  XStrBuilder *out;
  bool written;

  if (editor == NULL || state == NULL || !state->context_target.is_directory)
  {
    return false;
  }

  parent = state->context_target.path;
  if (!s_project_explorer_unique_child_path(
          &parent, "New Material", ".tml", &created) ||
      !ldk_material_desc_defaults(LDK_MATERIAL_TYPE_VERTEX_COLOR, &material))
  {
    return false;
  }

  context.assets = ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  out = x_strbuilder_create();
  if (out == NULL)
  {
    return false;
  }

  x_strbuilder_append(out, "material:\n");
  written = ldk_material_desc_write(&context, &material, out, 1, &result) &&
            x_io_write_text(created.buf, x_strbuilder_to_string(out));
  x_strbuilder_destroy(out);

  if (!written)
  {
    if (result.error[0] != 0)
    {
      ldki_editor_log_error(editor, result.error);
    }
    return false;
  }

  s_project_explorer_directory_select(state, &parent, true);
  state->selected_file = created;
  state->context_target.path = created;
  state->context_target.is_directory = false;
  state->context_target.surface = PROJECT_EXPLORER_SURFACE_FILES;
  s_project_explorer_rename_begin(state);
  return true;
}

static void s_project_explorer_context_menu_draw(
    LDKEditorContext *editor, ProjectExplorerState *state, LDKUIContext *ui)
{
  if (ldk_ui_begin_popup(ui, PROJECT_EXPLORER_CONTEXT_POPUP_ID))
  {
    bool is_directory = state->context_target.is_directory;
    bool is_root =
        x_fs_path_compare(&state->root, &state->context_target.path) == 0;

    if (ldk_ui_button_flat(ui, is_directory ? "Open Folder" : "Open"))
    {
      if (is_directory)
      {
        s_project_explorer_directory_select(
            state, &state->context_target.path, true);
      }
      else
      {
        ProjectExplorerEntry entry = {0};
        entry.path = state->context_target.path;
        s_project_explorer_on_file_double_click(editor, &entry);
      }
      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Copy Path"))
    {
      if (!ldk_os_clipboard_text_set(
              editor->window, state->context_target.path.buf))
      {
        ldki_editor_log_error(editor, "Failed to copy path.");
      }
      ldk_ui_close_current_popup(ui);
    }

    if (editor->project.loaded)
    {
      bool can_package = !editor->project_build.active &&
                         editor->editor_state == LDK_EDITOR_STATE_STOPED &&
                         ldki_editor_package_catalog_path_is_packageable(
                             editor, &state->context_target.path);
      u32 package_count = ldki_editor_package_catalog_count(editor);

      ldk_ui_horizontal_line(ui);
      ldk_ui_label(ui, "Add to Package...");
      ldk_ui_begin_disabled(ui, !can_package);
      for (u32 package_i = 0; package_i < package_count; ++package_i)
      {
        const char *package_name =
            ldki_editor_package_catalog_name_get(editor, package_i);
        if (package_name && ldk_ui_button_flat(ui, package_name))
        {
          if (!ldki_editor_package_catalog_add_path(editor, package_i,
                  &state->context_target.path, is_directory))
          {
            ldki_editor_log_error(
                editor, "Failed to add path to package.");
          }
          ldk_ui_close_current_popup(ui);
        }
      }
      ldk_ui_end_disabled(ui);

      if (package_count == 0)
      {
        ldk_ui_label(ui, "No packages defined.");
      }

      if (ldk_ui_button_flat(ui, "Manage Packages..."))
      {
        ldki_editor_package_catalog_open(editor);
        ldk_ui_close_current_popup(ui);
      }
      ldk_ui_horizontal_line(ui);
    }

    ldk_ui_begin_disabled(ui, is_root);

    if (ldk_ui_button_flat(ui, "Duplicate"))
    {
      XFSPath duplicate = {0};

      if (!s_project_explorer_path_duplicate(
              &state->context_target.path, is_directory, &duplicate))
      {
        ldki_editor_log_error(editor, "Failed to duplicate path.");
      }
      else if (!is_directory)
      {
        state->selected_file = duplicate;
      }
      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Rename"))
    {
      s_project_explorer_rename_begin(state);
      ldk_ui_close_current_popup(ui);
    }

    if (ldk_ui_button_flat(ui, "Delete"))
    {
      const char *message = is_directory
                                ? "Delete this folder and all its contents?"
                                : "Delete this file?";

      if (ldk_os_dialog_show_yes_no(editor->window, "Delete path?", message))
      {
        bool deleted = is_directory
                           ? x_fs_directory_delete_recursive(
                                 state->context_target.path.buf)
                           : x_fs_file_delete(state->context_target.path.buf);

        if (!deleted)
        {
          ldki_editor_log_error(editor, "Failed to delete path.");
        }
        else if (is_directory)
        {
          XFSPath parent = {0};
          if (x_fs_path_dirname(&state->context_target.path, &parent) > 0)
          {
            s_project_explorer_directory_select(state, &parent, false);
          }
          x_array_clear(state->expanded_paths);
        }
        else if (x_fs_path_compare(
                     &state->selected_file, &state->context_target.path) == 0)
        {
          memset(&state->selected_file, 0, sizeof(state->selected_file));
        }
      }
      ldk_ui_close_current_popup(ui);
    }
    ldk_ui_end_disabled(ui);

    if (is_directory)
    {
      ldk_ui_horizontal_line(ui);
      if (ldk_ui_button_flat(ui, "Create Directory"))
      {
        if (!s_project_explorer_create_directory(editor, state))
        {
          ldki_editor_log_error(editor, "Failed to create directory.");
        }
        ldk_ui_close_current_popup(ui);
      }

      bool can_create_asset = editor->project.loaded &&
                              editor->editor_state == LDK_EDITOR_STATE_STOPED;
      ldk_ui_begin_disabled(ui, !can_create_asset);
      if (ldk_ui_button_flat(ui, "New Scene"))
      {
        if (!s_project_explorer_create_scene(editor, state))
        {
          ldki_editor_log_error(editor, "Failed to create scene.");
        }
        ldk_ui_close_current_popup(ui);
      }

      if (ldk_ui_button_flat(ui, "New Material"))
      {
        if (!s_project_explorer_create_material(editor, state))
        {
          ldki_editor_log_error(editor, "Failed to create material.");
        }
        ldk_ui_close_current_popup(ui);
      }
      ldk_ui_end_disabled(ui);

      ldk_ui_horizontal_line(ui);

      if (ldk_ui_button_flat(ui, "Open Explorer here"))
      {
        ldk_ui_close_current_popup(ui);
        s_project_open_default(editor, state->context_target.path.buf);
      }
    }
    else
    {
      if (ldk_ui_button_flat(ui, "Open default program"))
      {
        ldk_ui_close_current_popup(ui);
        s_project_open_default(editor, state->context_target.path.buf);
      }
    }
    ldk_ui_end_popup(ui);
  }
}

static u32 s_project_explorer_text_prefix_fit(    LDKFontInstance *font, const char *text, float max_width)
{
  const char *cursor = text;
  const char *last_fit = text;

  if (font == NULL || text == NULL || max_width <= 0.0f)
  {
    return 0;
  }

  while (*cursor != '\0')
  {
    const char *next = cursor;
    u32 codepoint = 0;

    if (!ldk_ttf_utf8_consume_codepoint(&next, &codepoint) || next <= cursor)
    {
      break;
    }

    u32 byte_count = (u32)(next - text);
    LDKTextSize text_size = ldk_ttf_measure_text_cstrn(font, text, byte_count);

    if (text_size.w > max_width)
    {
      break;
    }

    last_fit = next;
    cursor = next;
  }

  return (u32)(last_fit - text);
}

static void s_project_explorer_text_copy(
    char *destination, u32 capacity, const char *source, u32 length)
{
  if (destination == NULL || capacity == 0)
  {
    return;
  }

  destination[0] = '\0';

  if (source == NULL)
  {
    return;
  }

  if (length >= capacity)
  {
    length = capacity - 1;
  }

  memcpy(destination, source, length);
  destination[length] = '\0';
}

static u32 s_project_explorer_tile_text_wrap(LDKUIContext *ui, const char *text,
    float max_width, char *first_line, u32 first_capacity, char *second_line,
    u32 second_capacity)
{
  static const char ellipsis[] = "...";
  const u32 ellipsis_length = (u32)(sizeof(ellipsis) - 1);

  if (first_line == NULL || first_capacity == 0 || second_line == NULL ||
      second_capacity == 0)
  {
    return 0;
  }

  first_line[0] = '\0';
  second_line[0] = '\0';

  if (ui == NULL || ui->font == NULL || text == NULL || text[0] == '\0')
  {
    return 0;
  }

  u32 text_length = (u32)strlen(text);
  LDKTextSize text_size = ldk_ttf_measure_text_cstr(ui->font, text);

  if (text_size.w <= max_width)
  {
    s_project_explorer_text_copy(first_line, first_capacity, text, text_length);
    return 1;
  }

  u32 first_length =
      s_project_explorer_text_prefix_fit(ui->font, text, max_width);
  s_project_explorer_text_copy(first_line, first_capacity, text, first_length);

  const char *remaining = text + first_length;
  while (*remaining == ' ' || *remaining == '\t')
  {
    remaining++;
  }

  if (*remaining == '\0')
  {
    return first_line[0] != '\0' ? 1 : 0;
  }

  u32 remaining_length = (u32)strlen(remaining);
  text_size = ldk_ttf_measure_text_cstr(ui->font, remaining);

  if (text_size.w <= max_width)
  {
    s_project_explorer_text_copy(
        second_line, second_capacity, remaining, remaining_length);
    return first_line[0] != '\0' ? 2 : 1;
  }

  LDKTextSize ellipsis_size = ldk_ttf_measure_text_cstr(ui->font, ellipsis);
  float second_width = max_width - ellipsis_size.w;
  u32 second_length =
      s_project_explorer_text_prefix_fit(ui->font, remaining, second_width);
  u32 second_prefix_capacity = second_capacity - 1;

  if (second_prefix_capacity >= ellipsis_length)
  {
    second_prefix_capacity -= ellipsis_length;
  }
  else
  {
    second_prefix_capacity = 0;
  }

  if (second_length > second_prefix_capacity)
  {
    second_length = second_prefix_capacity;

    while (
        second_length > 0 && (((u8)remaining[second_length] & 0xC0u) == 0x80u))
    {
      second_length--;
    }
  }

  s_project_explorer_text_copy(
      second_line, second_capacity, remaining, second_length);

  if (second_capacity - (u32)strlen(second_line) > ellipsis_length)
  {
    strcat(second_line, ellipsis);
  }

  return first_line[0] != '\0' ? 2 : 1;
}

static void s_project_explorer_tile_label_draw(LDKUIContext *ui,
    const char *text, LDKUIRect tile_rect, float label_y, float line_height)
{
  char lines[PROJECT_EXPLORER_TILE_LABEL_LINE_COUNT]
            [sizeof(((ProjectExplorerEntry *)0)->name.buf)] = {0};
  float max_width = tile_rect.w - LDK_UI_DEFAULT_SPACING * 2.0f;
  u32 line_count = s_project_explorer_tile_text_wrap(ui, text, max_width,
      lines[0], (u32)sizeof(lines[0]), lines[1], (u32)sizeof(lines[1]));

  for (u32 line_i = 0; line_i < line_count; line_i++)
  {
    LDKTextSize text_size = ldk_ttf_measure_text_cstr(ui->font, lines[line_i]);
    LDKUIRect line_rect = {
        tile_rect.x + (tile_rect.w - text_size.w) * 0.5f,
        label_y + line_height * (float)line_i,
        text_size.w,
        line_height,
    };

    ldk_ui_widget_label(ui, 0, lines[line_i], line_rect);
  }
}

static bool s_project_explorer_entry_selected(
    ProjectExplorerState *state, const ProjectExplorerEntry *entry)
{
  return state->selected_file.length != 0 &&
         state->last_click_package == entry->package &&
         x_fs_path_compare(&state->selected_file, &entry->path) == 0;
}

static bool s_project_explorer_file_button(LDKUIContext *ui,
    ProjectExplorerState *state, const ProjectExplorerEntry *entry,
    const char *text)
{
  if (!s_project_explorer_entry_selected(state, entry))
    return ldk_ui_button_flat(ui, text);

  rgba32 background = ui->theme.colors[LDK_UI_COLOR_CONTROL_BG];
  ui->theme.colors[LDK_UI_COLOR_CONTROL_BG] =
      ui->theme.colors[LDK_UI_COLOR_FOCUS];
  bool clicked = ldk_ui_button(ui, text);
  ui->theme.colors[LDK_UI_COLOR_CONTROL_BG] = background;
  return clicked;
}

static void s_project_explorer_reveal_scroll(ProjectExplorerState *state,
    LDKUIContext *ui, const ProjectExplorerEntry *entry, LDKUIRect rect)
{
  if (!state->reveal_scroll_pending ||
      !s_project_explorer_entry_selected(state, entry)) return;

  LDKUIRect view = ui->clip_rect;
  if (view.h <= 0.0f) return;
  if (rect.y < view.y || rect.h > view.h)
    state->file_scroll.y = fmaxf(0.0f, state->file_scroll.y + rect.y - view.y);
  else if (rect.y + rect.h > view.y + view.h)
    state->file_scroll.y += rect.y + rect.h - view.y - view.h;
  else
    state->reveal_scroll_pending = false;
}

static ProjectExplorerTileResult s_project_explorer_tile(
    LDKEditorContext *editor, ProjectExplorerState *state, LDKUIContext *ui,
    const ProjectExplorerEntry *entry, LDKUIIcon icon, float tile_width,
    float tile_height, float line_height)
{
  ProjectExplorerTileResult result = {0};

  if (ui == NULL || entry == NULL)
  {
    return result;
  }

  ldk_ui_push_id_cstr(ui, x_fs_path_cstr(&entry->path));

  ldk_ui_set_next_size(ui, ldk_ui_px(tile_width), ldk_ui_px(tile_height));
  result.clicked = s_project_explorer_file_button(ui, state, entry, "");
  LDKUIRect tile_rect = ldk_ui_last_rect(ui);
  LDKUIRect tile_bounding_rect = ldk_ui_last_bounding_rect(ui);
  LDKUIId tile_id = ui->last_id;
  s_project_explorer_reveal_scroll(state, ui, entry, tile_bounding_rect);
  result.pressed = ui->mouse != NULL && ui->active_id == tile_id &&
                   ldk_os_mouse_button_down(
                       (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT);
  result.right_clicked = s_project_explorer_rect_button_down(
      ui, tile_bounding_rect, LDK_MOUSE_BUTTON_RIGHT);

  LDKUIRect icon_rect = {
      tile_rect.x + (tile_rect.w - icon.size.w) * 0.5f,
      tile_rect.y + LDK_UI_DEFAULT_SPACING,
      icon.size.w,
      icon.size.h,
  };

  LDKUIRect image_rect = icon_rect;
  if (s_project_explorer_rect_visible(tile_bounding_rect, ui->clip_rect))
  {
    ProjectExplorerThumbnail *thumbnail = s_project_explorer_thumbnail_request(
        &state->thumbnails, entry);
    icon = s_project_explorer_thumbnail_icon(editor->renderer, thumbnail, icon);
    image_rect.x += (image_rect.w - icon.size.w) * 0.5f;
    image_rect.y += (image_rect.h - icon.size.h) * 0.5f;
    image_rect.w = icon.size.w;
    image_rect.h = icon.size.h;
  }

  ldk_ui_widget_icon_label(ui, 0, icon, "", image_rect);

  float label_y = icon_rect.y + icon_rect.h + LDK_UI_DEFAULT_SPACING;
  if (s_project_explorer_rename_matches(
          state, &entry->path, PROJECT_EXPLORER_SURFACE_FILES))
  {
    LDKUIRect input_rect = {tile_rect.x + LDK_UI_DEFAULT_SPACING, label_y,
        tile_rect.w - LDK_UI_DEFAULT_SPACING * 2.0f, line_height};
    s_project_explorer_rename_input_draw(editor, state, ui,
        tile_id ^ PROJECT_EXPLORER_TILE_RENAME_INPUT_ID, input_rect);
  }
  else
  {
    s_project_explorer_tile_label_draw(
        ui, entry->name.buf, tile_rect, label_y, line_height);
  }

  ui->last_rect = tile_rect;
  ui->last_bounding_rect = tile_bounding_rect;
  ui->last_id = tile_id;

  ldk_ui_pop_id(ui);
  return result;
}

static void s_project_explorer_path_drag_source(
    LDKUIContext *ui, const ProjectExplorerEntry *entry, LDKUIIcon icon)
{
  if (ui == NULL || entry == NULL || ui->mouse == NULL ||
      ui->active_id != ui->last_id)
  {
    return;
  }

  LDKMouseState *mouse = (LDKMouseState *)ui->mouse;

  if (ldk_os_mouse_button_down(mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    if (entry->package_backed && !entry->is_directory && entry->asset_path.length)
    {
      XSmallstr payload = {0};
      x_smallstr_from_cstr(&payload, entry->asset_path.buf);
      ldk_ui_drag_n_drop_payload_set(
          LDK_EDITOR_DRAG_N_DROP_PAYLOAD_ASSET_PATH, &payload);
    }
    else if (!entry->package_backed)
    {
      ldk_ui_drag_n_drop_payload_set(
          LDK_EDITOR_DRAG_N_DROP_PAYLOAD_FILE_PATH, &entry->path);
    }
  }

  ldk_ui_drag_n_drop_preview_draw(ui, icon);
}

static bool s_project_explorer_entries_draw(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIIcon folder_icon,
    LDKUIIcon file_icon)
{
  u32 total_count = x_array_count(state->dirs) + x_array_count(state->files);
  bool compact_mode = state->icon_size <= PROJECT_EXPLORER_MIN_ICON_SIZE;
  bool right_click_handled = false;

  if (compact_mode)
  {
    for (u32 entry_i = 0; entry_i < total_count; entry_i++)
    {
      bool is_directory = false;
      ProjectExplorerEntry *entry =
          s_project_explorer_entry_get(state, entry_i, &is_directory);
      LDKUIIcon entry_icon = is_directory ? folder_icon : file_icon;
      LDKUIMark entry_mark = ldk_ui_mark(ui);

      if (!is_directory)
      {
        entry_icon.uv = ldk_editor_icon_rects[s_project_explorer_file_icon_get(
            &entry->path)];
      }

      ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
      ldk_ui_begin_horizontal(ui);

      ldk_ui_set_next_weight(ui, 0.0f);
      bool icon_clicked = ldk_ui_icon_button(ui, entry_icon, NULL);
      s_project_explorer_path_drag_source(
          ui, entry, entry_icon);
      bool renaming = s_project_explorer_rename_matches(
          state, &entry->path, PROJECT_EXPLORER_SURFACE_FILES);
      bool label_clicked = false;
      if (renaming)
      {
        s_project_explorer_rename_input_layout_draw(editor, state, ui);
      }
      else
      {
        label_clicked = s_project_explorer_file_button(
            ui, state, entry, entry->name.buf);
        s_project_explorer_path_drag_source(
            ui, entry, entry_icon);
      }

      ldk_ui_end_horizontal(ui);
      s_project_explorer_reveal_scroll(
          state, ui, entry, ldk_ui_measure_from(ui, entry_mark));

      bool right_clicked = s_project_explorer_rect_button_down(
          ui, ldk_ui_measure_from(ui, entry_mark), LDK_MOUSE_BUTTON_RIGHT);

      if (icon_clicked || label_clicked)
      {
        s_project_explorer_entry_activate(editor, state, entry, is_directory);
      }

      if (right_clicked && !entry->package_backed)
      {
        s_project_explorer_on_right_click(
            ui, state, entry, is_directory, PROJECT_EXPLORER_SURFACE_FILES);
        right_click_handled = true;
      }
    }

    return right_click_handled;
  }

  float tile_w = state->icon_size + 32.0f;
  float line_height = ldk_ttf_get_line_height(ui->font);
  float tile_h = state->icon_size +
                 line_height * PROJECT_EXPLORER_TILE_LABEL_LINE_COUNT +
                 LDK_UI_DEFAULT_SPACING * 3.0f;
  float available_w =
      ui->current_layout != NULL ? ui->current_layout->content_rect.w : tile_w;
  u32 column_count = (u32)(available_w / tile_w);
  if (column_count == 0)
  {
    column_count = 1;
  }
  u32 tile_index = 0;
  while (tile_index < total_count)
  {
    ldk_ui_set_next_height(ui, ldk_ui_px(tile_h));
    ldk_ui_begin_horizontal(ui);

    for (u32 column = 0; column < column_count && tile_index < total_count;
         column++, tile_index++)
    {
      bool is_directory = false;
      ProjectExplorerEntry *entry =
          s_project_explorer_entry_get(state, tile_index, &is_directory);
      LDKUIIcon entry_icon = is_directory ? folder_icon : file_icon;

      if (!is_directory)
      {
        entry_icon.uv = ldk_editor_icon_rects[s_project_explorer_file_icon_get(
            &entry->path)];
      }

      ProjectExplorerTileResult result = s_project_explorer_tile(
          editor, state, ui, entry, entry_icon, tile_w, tile_h, line_height);
      s_project_explorer_path_drag_source(
          ui, entry, entry_icon);

      if ((is_directory && result.clicked) || (!is_directory && result.pressed))
      {
        s_project_explorer_entry_activate(editor, state, entry, is_directory);
      }

      if (result.right_clicked && !entry->package_backed)
      {
        s_project_explorer_on_right_click(
            ui, state, entry, is_directory, PROJECT_EXPLORER_SURFACE_FILES);
        right_click_handled = true;
      }
    }

    ldk_ui_spacer(ui);
    ldk_ui_end_horizontal(ui);
  }

  s_project_explorer_thumbnail_process(&state->thumbnails);
  return right_click_handled;
}

static void s_project_explorer_files_draw(LDKEditorContext *editor,
    ProjectExplorerState *state, LDKUIContext *ui, LDKUIIcon folder_icon,
    LDKUIIcon file_icon)
{
  ldk_ui_begin_vertical(ui);

  //
  // File area Toolbar 
  //
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  LDKUIIcon up_dir_icon = {0};
  up_dir_icon.size = ldk_sizef(24, 24);
  up_dir_icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  up_dir_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_FOLDER_UP];
  up_dir_icon.color = ui->theme.colors[LDK_UI_COLOR_CONTROL_TEXT];

  ldk_ui_set_next_width(ui, ldk_ui_px(24.0f));

  XFSPath relative_directory = {0};
  const char* path = NULL;
  if (state->selected_package >= 0)
  {
    path = state->selected_package_directory.buf;
  }
  else if (x_fs_path_relative_to(
          &state->root, &state->selected_directory, &relative_directory) > 0)
  {
    path = relative_directory.buf;
  }
  else
  {
    path = state->selected_directory.buf;
  }

  // up one dir button
  bool toplevel = strncmp(path, ".", 1) == 0 || strlen(path) == 0;
  ldk_ui_begin_disabled(ui, toplevel);
  if (ldk_ui_icon_button(ui, up_dir_icon, NULL))
  {
    if (state->selected_package >= 0)
    {
      char parent[LDK_ASSET_PATH_MAX_LENGTH + 1];
      snprintf(parent, sizeof(parent), "%s", state->selected_package_directory.buf);
      char *slash = strrchr(parent, '/');
      if (slash) *slash = 0; else parent[0] = 0;
      x_smallstr_from_cstr(&state->selected_package_directory, parent);
      memset(&state->selected_file, 0, sizeof(state->selected_file));
    }
    else
    {
      XFSPath up_path = {0};
      x_fs_directory_parent(&state->selected_directory, &up_path);
      s_project_explorer_directory_select(state, &up_path, true);
    }
    return;
  }
  ldk_ui_end_disabled(ui);

  // full path label
  if (state->selected_package >= 0)
  {
    ProjectExplorerPackageMount *mount =
        x_array_get(state->package_mounts, (u32)state->selected_package);
    char package_path[X_SMALLSTR_MAX_LENGTH + LDK_ASSET_PATH_MAX_LENGTH + 4];
    snprintf(package_path, sizeof(package_path), "%s%s%s", mount->name.buf,
        path[0] ? "/" : "", path);
    ldk_ui_label(ui, package_path);
  }
  else
  {
    ldk_ui_label(ui, path);
  }
  ldk_ui_spacer(ui);

  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_set_next_width(ui, ldk_ui_px(160.0f));
  ldk_ui_set_next_weight(ui, 0.0f);
  state->icon_size = ldk_ui_slider_input(
      ui, state->icon_size, PROJECT_EXPLORER_MIN_ICON_SIZE, 72.0f);
  ldk_ui_end_horizontal(ui);

  //
  // File area content
  //

  folder_icon.size = ldk_sizef(state->icon_size, state->icon_size);
  file_icon.size = folder_icon.size;

  state->file_scroll = ldk_ui_begin_scrollview(
      ui, state->file_scroll, LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  LDKUIRect file_view_rect = ui->clip_rect;

  if (state->selected_package >= 0)
  {
    s_project_explorer_package_directory_read(state,
        (u32)state->selected_package, state->selected_package_directory.buf,
        state->dirs, state->files);
  }
  else
  {
    s_project_explorer_directory_read(
        &state->selected_directory, state->dirs, state->files);
  }
  bool right_click_handled = s_project_explorer_entries_draw(
      editor, state, ui, folder_icon, file_icon);

  if (state->selected_package < 0 && !right_click_handled &&
      s_project_explorer_rect_button_down(
          ui, file_view_rect, LDK_MOUSE_BUTTON_RIGHT))
  {
    ProjectExplorerEntry directory_entry = {0};
    directory_entry.path = state->selected_directory;
    x_fs_path_basename(&directory_entry.path, &directory_entry.name);
    s_project_explorer_on_right_click(
        ui, state, &directory_entry, true, PROJECT_EXPLORER_SURFACE_TREE);
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
  s_project_explorer_context_menu_draw(editor, state, ui);
  ldk_ui_end_vertical(ui);
}

static void s_editor_project_explorer(
    LDKEditorContext *editor, const char *root_path)
{
  ProjectExplorerState *state = &s_project_explorer_state;
  LDKUIContext *ui = &editor->ui;
  bool owns_window = ui->current_window == NULL;

  if (owns_window)
  {
    state->window_rect = ldk_ui_begin_window(
        ui, "Project Explorer", state->window_rect, LDK_UI_WINDOW_TOOL);
  }

  if (!s_project_explorer_initialize(state))
  {
    ldk_ui_label(ui, "Project explorer allocation failed.");

    if (owns_window)
    {
      ldk_ui_end_window(ui);
    }
    return;
  }

  s_project_explorer_thumbnail_frame_begin(&state->thumbnails, editor->renderer);

  if (!s_project_explorer_root_set(state, root_path))
  {
    ldk_ui_label(ui, "No project root.");

    if (owns_window)
    {
      ldk_ui_end_window(ui);
    }
    return;
  }

  s_project_explorer_rename_before_draw(editor, state, ui);

  LDKUIIcon file_icon = {0};
  file_icon.size = ldk_sizef(state->icon_size, state->icon_size);
  file_icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);
  file_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_FILE];
  file_icon.color = LDK_EDITOR_COLOR_FILE;

  LDKUIIcon folder_icon = file_icon;
  folder_icon.uv = ldk_editor_icon_rects[LDK_EDITOR_ICON_FOLDER];
  folder_icon.color = LDK_EDITOR_COLOR_FOLDER;

  LDKUIIcon tree_folder_icon = folder_icon;
  tree_folder_icon.size = ldk_sizef(
      PROJECT_EXPLORER_TREE_ICON_SIZE, PROJECT_EXPLORER_TREE_ICON_SIZE);

  ldk_ui_begin_horizontal(ui);
  {
    float spacing = ui->current_layout != NULL
                        ? ui->current_layout->spacing
                        : LDK_UI_DEFAULT_SPACING;
    float max_tree_width = ui->current_layout != NULL
                               ? ui->current_layout->content_rect.w -
                                     PROJECT_EXPLORER_FILES_WIDTH_MIN -
                                     2.0f * spacing
                               : state->tree_width;

    if (max_tree_width < PROJECT_EXPLORER_TREE_WIDTH_MIN)
    {
      max_tree_width = PROJECT_EXPLORER_TREE_WIDTH_MIN;
    }
    if (state->tree_width < PROJECT_EXPLORER_TREE_WIDTH_MIN)
    {
      state->tree_width = PROJECT_EXPLORER_TREE_WIDTH_MIN;
    }
    if (state->tree_width > max_tree_width)
    {
      state->tree_width = max_tree_width;
    }

    ldk_ui_set_next_width(ui, ldk_ui_px(state->tree_width));
    s_project_explorer_tree_draw(editor, state, ui, tree_folder_icon);
    state->tree_width = ldk_ui_resize_handle_vertical(ui, state->tree_width,
        PROJECT_EXPLORER_TREE_WIDTH_MIN, max_tree_width);
    s_project_explorer_files_draw(editor, state, ui, folder_icon, file_icon);
  }
  ldk_ui_end_horizontal(ui);

  if (owns_window)
  {
    ldk_ui_end_window(ui);
  }
}

void ldk_editor_file_explorer_show(LDKEditor *editor, const char *root_path)
{
  LDKEditorContext *context = (LDKEditorContext *)editor;
  ldki_editor_package_catalog_sync(context);
  s_editor_project_explorer(context, root_path);
}
