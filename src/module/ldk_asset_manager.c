#include <ldk_asset_manager.h>
#include <stdx/stdx_io.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static LDKAssetHandle s_ldk_asset_handle_from_x(XHandle h)
{
  LDKAssetHandle out = { h };
  return out;
}

static LDKAssetTextFile s_ldk_asset_text_file_from_x(XHandle h)
{
  LDKAssetTextFile out = { h };
  return out;
}

static void s_ldk_asset_info_destroy(LDKAssetInfo* info)
{
  if (!info)
  {
    return;
  }

  if (info->type == LDK_ASSET_TYPE_TEXT_FILE)
  {
    LDKAssetTextFileData* data = (LDKAssetTextFileData*)info->data;

    if (data)
    {
      if (data->text)
      {
        free(data->text);
      }

      free(data);
    }
  }
  else if (info->type == LDK_ASSET_TYPE_FONT)
  {
    LDKAssetFontData* data = (LDKAssetFontData*)info->data;

    if (data)
    {
      if (data->face)
      {
        ldk_font_face_destroy(data->face);
      }

      free(data);
    }
  }

  memset(info, 0, sizeof(*info));
}

static void s_ldk_asset_ctor(void* user, void* item)
{
  (void)user;

  memset(item, 0, sizeof(LDKAssetInfo));
}

static void s_ldk_asset_dtor(void* user, void* item)
{
  (void)user;

  s_ldk_asset_info_destroy((LDKAssetInfo*)item);
}

bool ldk_asset_manager_initialize(LDKAssetManager* manager, u32 page_capacity, u32 initial_pages)
{
  if (!manager)
  {
    return false;
  }

  memset(manager, 0, sizeof(*manager));

  XHPoolConfig config = {0};
  config.page_capacity = page_capacity ? page_capacity : 1024;
  config.initial_pages = initial_pages ? initial_pages : 1;

  return x_hpool_init(
      &manager->pool,
      sizeof(LDKAssetInfo),
      config,
      s_ldk_asset_ctor,
      s_ldk_asset_dtor,
      NULL) != 0;
}

void ldk_asset_manager_terminate(LDKAssetManager* manager)
{
  if (!manager)
  {
    return;
  }

  x_hpool_term(&manager->pool);
  memset(manager, 0, sizeof(*manager));
}

void ldk_asset_manager_clear(LDKAssetManager* manager)
{
  if (!manager)
  {
    return;
  }

  x_hpool_clear(&manager->pool);
}

LDKAssetHandle ldk_asset_handle_null(void)
{
  return s_ldk_asset_handle_from_x(x_handle_null());
}


LDKAssetImage ldk_asset_image_null(void)
{
  LDKAssetImage out = { x_handle_null() };
  return out;
}

LDKAssetFont ldk_asset_font_null(void)
{
  LDKAssetFont out = { x_handle_null() };
  return out;
}

LDKAssetMesh ldk_asset_mesh_null(void)
{
  LDKAssetMesh out = { x_handle_null() };
  return out;
}

bool ldk_asset_handle_is_alive(LDKAssetManager* manager, LDKAssetHandle asset)
{
  if (!manager)
  {
    return false;
  }

  return x_hpool_is_alive(&manager->pool, asset.h) != 0;
}

LDKAssetInfo* ldk_asset_get_info(LDKAssetManager* manager, LDKAssetHandle asset)
{
  if (!manager)
  {
    return NULL;
  }

  return (LDKAssetInfo*)x_hpool_get(&manager->pool, asset.h);
}

const LDKAssetInfo* ldk_asset_get_info_const(LDKAssetManager* manager, LDKAssetHandle asset)
{
  return ldk_asset_get_info(manager, asset);
}

LDKAssetType ldk_asset_get_type(LDKAssetManager* manager, LDKAssetHandle asset)
{
  LDKAssetInfo* info = ldk_asset_get_info(manager, asset);

  if (!info)
  {
    return LDK_ASSET_TYPE_NULL;
  }

  return info->type;
}

u32 ldk_asset_alive_count(LDKAssetManager* manager)
{
  if (!manager)
  {
    return 0;
  }

  return x_hpool_alive_count(&manager->pool);
}

void ldk_asset_foreach(LDKAssetManager* manager, LDKAssetIterFn fn, void* user)
{
  if (!manager)
  {
    return;
  }

  if (!fn)
  {
    return;
  }

  XHPoolIter it = {0};
  XHandle h = x_handle_null();

  for (
      LDKAssetInfo* info = (LDKAssetInfo*)x_hpool_iter_begin(&manager->pool, &it, &h);
      info;
      info = (LDKAssetInfo*)x_hpool_iter_next(&manager->pool, &it, &h))
  {
    if (!fn(s_ldk_asset_handle_from_x(h), info, user))
    {
      break;
    }
  }
}

// 
// Text file
//

LDKAssetTextFile ldk_asset_text_file_null(void)
{
  return s_ldk_asset_text_file_from_x(x_handle_null());
}

bool ldk_asset_manager_text_file_is_alive(LDKAssetManager* manager, LDKAssetTextFile asset)
{
  LDKAssetHandle generic = { asset.h };

  if (!ldk_asset_handle_is_alive(manager, generic))
  {
    return false;
  }

  return ldk_asset_get_type(manager, generic) == LDK_ASSET_TYPE_TEXT_FILE;
}

LDKAssetTextFile ldk_asset_manager_text_file_create(LDKAssetManager* manager, const char* text, u64 byte_count)
{
  LDKAssetTextFile result = ldk_asset_text_file_null();

  if (!manager)
  {
    return result;
  }

  if (!text && byte_count > 0)
  {
    return result;
  }

  char* copy = (char*)malloc((size_t)byte_count + 1);

  if (!copy)
  {
    return result;
  }

  if (byte_count > 0)
  {
    memcpy(copy, text, (size_t)byte_count);
  }

  copy[byte_count] = 0;

  LDKAssetTextFileData* data = (LDKAssetTextFileData*)malloc(sizeof(LDKAssetTextFileData));

  if (!data)
  {
    free(copy);
    return result;
  }

  data->text = copy;
  data->byte_count = byte_count;

  XHandle h = x_hpool_alloc(&manager->pool);

  if (x_handle_is_null(h))
  {
    free(copy);
    free(data);
    return result;
  }

  LDKAssetInfo* info = (LDKAssetInfo*)x_hpool_get(&manager->pool, h);

  if (!info)
  {
    free(copy);
    free(data);
    x_hpool_free(&manager->pool, h);
    return result;
  }

  info->type = LDK_ASSET_TYPE_TEXT_FILE;
  info->data = data;

#ifdef LDK_DEBUG
  info->asset_path.buf[0] = 0;
  info->asset_path.length = 0;
  info->load_timestamp = (u64)time(NULL);
#endif

  return s_ldk_asset_text_file_from_x(h);
}

LDKAssetTextFile ldk_asset_manager_text_file_load(LDKAssetManager* manager, const char* path)
{
  LDKAssetTextFile result = ldk_asset_text_file_null();
  size_t text_file_size = 0;
  char* text_file_data = x_io_read_text(path, &text_file_size);
  result = ldk_asset_manager_text_file_create(manager, text_file_data, (u32)text_file_size);
  free(text_file_data);

#ifdef LDK_DEBUG
  LDKAssetInfo* info = ldk_asset_get_info(manager, s_ldk_asset_handle_from_x(result.h));

  if (info)
  {
    x_fs_path_set(&info->asset_path, path);
    info->load_timestamp = (u64)time(NULL);
  }
#endif

  return result;
}

void ldk_asset_manager_text_file_unload(LDKAssetManager* manager, LDKAssetTextFile asset)
{
  if (!manager)
  {
    return;
  }

  if (!ldk_asset_manager_text_file_is_alive(manager, asset))
  {
    return;
  }

  x_hpool_free(&manager->pool, asset.h);
}

LDKAssetTextFileData* ldk_asset_manager_text_file_get(LDKAssetManager* manager, LDKAssetTextFile asset)
{
  LDKAssetHandle generic = { asset.h };
  LDKAssetInfo* info = ldk_asset_get_info(manager, generic);

  if (!info)
  {
    return NULL;
  }

  if (info->type != LDK_ASSET_TYPE_TEXT_FILE)
  {
    return NULL;
  }

  return (LDKAssetTextFileData*)info->data;
}

const LDKAssetTextFileData* ldk_asset_manager_text_file_get_const(LDKAssetManager* manager, LDKAssetTextFile asset)
{
  return ldk_asset_manager_text_file_get(manager, asset);
}

//
// Font
//

static LDKAssetFont s_ldk_asset_font_from_x(XHandle h)
{
  LDKAssetFont out = { h };
  return out;
}

bool ldk_asset_manager_font_is_alive(LDKAssetManager* manager, LDKAssetFont asset)
{
  LDKAssetHandle generic = { asset.h };

  if (!ldk_asset_handle_is_alive(manager, generic))
  {
    return false;
  }

  return ldk_asset_get_type(manager, generic) == LDK_ASSET_TYPE_FONT;
}

LDKAssetFont ldk_asset_manager_font_create(LDKAssetManager* manager, const void* data, u32 data_size)
{
  LDKAssetFont result = ldk_asset_font_null();

  if (!manager || !data || data_size == 0)
  {
    return result;
  }

  LDKFontFace* face = ldk_font_face_create(data, data_size);

  if (!face)
  {
    return result;
  }

  LDKAssetFontData* font_data = (LDKAssetFontData*)malloc(sizeof(LDKAssetFontData));

  if (!font_data)
  {
    ldk_font_face_destroy(face);
    return result;
  }

  font_data->face = face;

  XHandle h = x_hpool_alloc(&manager->pool);

  if (x_handle_is_null(h))
  {
    ldk_font_face_destroy(face);
    free(font_data);
    return result;
  }

  LDKAssetInfo* info = (LDKAssetInfo*)x_hpool_get(&manager->pool, h);

  if (!info)
  {
    x_hpool_free(&manager->pool, h);
    ldk_font_face_destroy(face);
    free(font_data);
    return result;
  }

  info->type = LDK_ASSET_TYPE_FONT;
  info->data = font_data;

#ifdef LDK_DEBUG
  info->asset_path.buf[0] = 0;
  info->asset_path.length = 0;
  info->load_timestamp = (u64)time(NULL);
#endif

  return s_ldk_asset_font_from_x(h);
}

LDKAssetFont ldk_asset_manager_font_load(LDKAssetManager* manager, const char* path)
{
  LDKAssetFont result = ldk_asset_font_null();

  if (!manager || !path)
  {
    return result;
  }

  XFile* file = x_io_open(path, "rb");
  if (!file)
  {
    return result;
  }

  size_t font_file_size = 0;
  const char* font_file_data = x_io_read_all(file, &font_file_size);
  if (!font_file_data)
  {
    return result;
  }
  x_io_close(file);

  result = ldk_asset_manager_font_create(manager, font_file_data, (u32)font_file_size);
  free((void*)font_file_data);

#ifdef LDK_DEBUG
  LDKAssetHandle generic = { result.h };
  LDKAssetInfo* info = ldk_asset_get_info(manager, generic);

  if (info)
  {
    x_fs_path_set(&info->asset_path, path);
    info->load_timestamp = (u64)time(NULL);
  }
#endif

  return result;
}

void ldk_asset_manager_font_unload(LDKAssetManager* manager, LDKAssetFont asset)
{
  if (!manager)
  {
    return;
  }

  if (!ldk_asset_manager_font_is_alive(manager, asset))
  {
    return;
  }

  x_hpool_free(&manager->pool, asset.h);
}

LDKAssetFontData* ldk_asset_manager_font_get(LDKAssetManager* manager, LDKAssetFont asset)
{
  LDKAssetHandle generic = { asset.h };
          LDKAssetInfo* info = ldk_asset_get_info(manager, generic);

  if (!info || info->type != LDK_ASSET_TYPE_FONT)
  {
    return NULL;
  }

  return (LDKAssetFontData*)info->data;
}

const LDKAssetFontData* ldk_asset_manager_font_get_const(LDKAssetManager* manager, LDKAssetFont asset)
{
  return ldk_asset_manager_font_get(manager, asset);
}
