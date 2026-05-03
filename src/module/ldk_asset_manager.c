#include <module/ldk_asset_manager.h>
#include <stdx/stdx_io.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static LDKAssetHandle s_asset_handle_from_x(XHandle h)
{
  LDKAssetHandle out = { h };
  return out;
}

static LDKAssetTextFile s_asset_text_file_from_x(XHandle h)
{
  LDKAssetTextFile out = { h };
  return out;
}

static void s_asset_info_destroy(LDKAssetInfo* info)
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
  else if (info->type == LDK_ASSET_TYPE_IMAGE)
  {
    LDKAssetImageData* data = (LDKAssetImageData*)info->data;

    if (data)
    {
      if (data->image)
      {
        ldk_image_destroy(data->image);
      }

      free(data);
    }
  }
  else if (info->type == LDK_ASSET_TYPE_MESH)
  {
    LDKAssetMeshData* data = (LDKAssetMeshData*)info->data;

    if (data)
    {
      free(data->mesh.vertices);
      free(data->mesh.indices);



      free(data);
    }
  }

  memset(info, 0, sizeof(*info));
}

static void s_asset_ctor(void* user, void* item)
{
  (void)user;

  memset(item, 0, sizeof(LDKAssetInfo));
}

static void s_asset_dtor(void* user, void* item)
{
  (void)user;

  s_asset_info_destroy((LDKAssetInfo*)item);
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
      s_asset_ctor,
      s_asset_dtor,
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
  return s_asset_handle_from_x(x_handle_null());
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
    if (!fn(s_asset_handle_from_x(h), info, user))
    {
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Text file asset
// ---------------------------------------------------------------------------

LDKAssetTextFile ldk_asset_text_file_null(void)
{
  return s_asset_text_file_from_x(x_handle_null());
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

  return s_asset_text_file_from_x(h);
}

LDKAssetTextFile ldk_asset_manager_text_file_load(LDKAssetManager* manager, const char* path)
{
  LDKAssetTextFile result = ldk_asset_text_file_null();
  size_t text_file_size = 0;
  char* text_file_data = x_io_read_text(path, &text_file_size);
  result = ldk_asset_manager_text_file_create(manager, text_file_data, (u32)text_file_size);
  free(text_file_data);

#ifdef LDK_DEBUG
  LDKAssetInfo* info = ldk_asset_get_info(manager, s_asset_handle_from_x(result.h));

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


// ---------------------------------------------------------------------------
// Image asset
// ---------------------------------------------------------------------------

static LDKAssetImage s_ldk_asset_image_from_handle(XHandle h)
{
  LDKAssetImage out = { h };
  return out;
}

LDKAssetImage ldk_asset_image_null(void)
{
  LDKAssetImage out = { x_handle_null() };
  return out;
}

bool ldk_asset_manager_image_is_alive(LDKAssetManager* manager, LDKAssetImage asset)
{
  LDKAssetHandle generic = { asset.h };

  if (!ldk_asset_handle_is_alive(manager, generic))
  {
    return false;
  }

  return ldk_asset_get_type(manager, generic) == LDK_ASSET_TYPE_IMAGE;
}

LDKAssetImage ldk_asset_manager_image_create(LDKAssetManager* manager, u32 width, u32 height, const void* pixels)
{
  LDKAssetImage result = ldk_asset_image_null();

  if (!manager || !pixels)
  {
    return result;
  }

  LDKImage* image = ldk_image_create(width, height, pixels);

  if (!image)
  {
    return result;
  }

  LDKAssetImageData* image_data = malloc(sizeof(LDKAssetImageData));

  if (!image_data)
  {
    ldk_image_destroy(image);
    return result;
  }

  image_data->image = image;

  XHandle h = x_hpool_alloc(&manager->pool);

  if (x_handle_is_null(h))
  {
    ldk_image_destroy(image);
    free(image_data);
    return result;
  }

  LDKAssetInfo* info = x_hpool_get(&manager->pool, h);

  if (!info)
  {
    x_hpool_free(&manager->pool, h);
    ldk_image_destroy(image);
    free(image_data);
    return result;
  }

  info->type = LDK_ASSET_TYPE_IMAGE;
  info->data = image_data;

#ifdef LDK_DEBUG
  info->asset_path.buf[0] = 0;
  info->asset_path.length = 0;
  info->load_timestamp = (u64)time(NULL);
#endif

  return s_ldk_asset_image_from_handle(h);
}

LDKAssetImage ldk_asset_manager_image_load(LDKAssetManager* manager, const char* path)
{
  LDKAssetImage result = ldk_asset_image_null();

  if (!manager || !path)
  {
    return result;
  }

  LDKImage* image = ldk_image_load(path);

  if (!image)
  {
    return result;
  }

  LDKAssetImageData* image_data = malloc(sizeof(LDKAssetImageData));

  if (!image_data)
  {
    ldk_image_destroy(image);
    return result;
  }

  image_data->image = image;

  XHandle h = x_hpool_alloc(&manager->pool);

  if (x_handle_is_null(h))
  {
    ldk_image_destroy(image);
    free(image_data);
    return result;
  }

  LDKAssetInfo* info = x_hpool_get(&manager->pool, h);

  if (!info)
  {
    x_hpool_free(&manager->pool, h);
    ldk_image_destroy(image);
    free(image_data);
    return result;
  }

  info->type = LDK_ASSET_TYPE_IMAGE;
  info->data = image_data;

#ifdef LDK_DEBUG
  x_fs_path_set(&info->asset_path, path);
  info->load_timestamp = (u64)time(NULL);
#endif

  return s_ldk_asset_image_from_handle(h);
}

void ldk_asset_manager_image_unload(LDKAssetManager* manager, LDKAssetImage asset)
{
  if (!manager)
  {
    return;
  }

  if (!ldk_asset_manager_image_is_alive(manager, asset))
  {
    return;
  }

  x_hpool_free(&manager->pool, asset.h);
}

LDKAssetImageData* ldk_asset_manager_image_get(LDKAssetManager* manager, LDKAssetImage asset)
{
  LDKAssetHandle generic = { asset.h };
  LDKAssetInfo* info = ldk_asset_get_info(manager, generic);

  if (!info || info->type != LDK_ASSET_TYPE_IMAGE)
  {
    return NULL;
  }

  return (LDKAssetImageData*)info->data;
}

const LDKAssetImageData* ldk_asset_manager_image_get_const(LDKAssetManager* manager, LDKAssetImage asset)
{
  return ldk_asset_manager_image_get(manager, asset);
}


// ---------------------------------------------------------------------------
// Font asset
// ---------------------------------------------------------------------------

static LDKAssetFont s_asset_font_from_handle(XHandle h)
{
  LDKAssetFont out = { h };
  return out;
}

LDKAssetFont ldk_asset_font_null(void)
{
  LDKAssetFont out = { x_handle_null() };
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

  return s_asset_font_from_handle(h);
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
  x_io_close(file);

  if (!font_file_data)
  {
    return result;
  }

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

// ---------------------------------------------------------------------------
// Mesh asset
// ---------------------------------------------------------------------------

static LDKAssetMesh s_asset_mesh_from_handle(XHandle h)
{
  LDKAssetMesh out = { h };
  return out;
}

LDKAssetMesh ldk_asset_mesh_null(void)
{
  LDKAssetMesh out = { x_handle_null() };
  return out;
}

bool ldk_asset_manager_mesh_is_alive(LDKAssetManager* manager, LDKAssetMesh asset)
{
  LDKAssetHandle generic = { asset.h };

  if (!ldk_asset_handle_is_alive(manager, generic))
  {
    return false;
  }

  return ldk_asset_get_type(manager, generic) == LDK_ASSET_TYPE_MESH;
}

LDKAssetMesh ldk_asset_manager_mesh_create(LDKAssetManager* manager,
    const LDKMeshVertex* vertices, u32 vertex_count, const u32* indices, u32 index_count)
{
  LDKAssetMesh result = ldk_asset_mesh_null();

  if (!manager)
  {
    return result;
  }

  if (!vertices || vertex_count == 0)
  {
    return result;
  }

  if (!indices || index_count == 0)
  {
    return result;
  }

  LDKAssetMeshData* data = (LDKAssetMeshData*)malloc(sizeof(LDKAssetMeshData));

  if (!data)
  {
    return result;
  }

  memset(data, 0, sizeof(*data));

  size_t vertex_bytes = (size_t)vertex_count * sizeof(LDKMeshVertex);
  size_t index_bytes = (size_t)index_count * sizeof(u32);

  data->mesh.vertices = (LDKMeshVertex*)malloc(vertex_bytes);

  if (!data->mesh.vertices)
  {
    free(data);
    return result;
  }

  data->mesh.indices = (u32*)malloc(index_bytes);

  if (!data->mesh.indices)
  {
    free(data->mesh.vertices);
    free(data);
    return result;
  }

  memcpy(data->mesh.vertices, vertices, vertex_bytes);
  memcpy(data->mesh.indices, indices, index_bytes);

  data->mesh.vertex_count = vertex_count;
  data->mesh.index_count = index_count;

  XHandle h = x_hpool_alloc(&manager->pool);

  if (x_handle_is_null(h))
  {
    free(data->mesh.vertices);
    free(data->mesh.indices);
    free(data);
    return result;
  }

  LDKAssetInfo* info = (LDKAssetInfo*)x_hpool_get(&manager->pool, h);

  if (!info)
  {
    x_hpool_free(&manager->pool, h);
    free(data->mesh.vertices);
    free(data->mesh.indices);
    free(data);
    return result;
  }

  info->type = LDK_ASSET_TYPE_MESH;
  info->data = data;

#ifdef LDK_DEBUG
  info->asset_path.buf[0] = 0;
  info->asset_path.length = 0;
  info->load_timestamp = (u64)time(NULL);
#endif

  return s_asset_mesh_from_handle(h);
}

void ldk_asset_manager_mesh_unload(LDKAssetManager* manager, LDKAssetMesh asset)
{
  if (!manager)
  {
    return;
  }

  if (!ldk_asset_manager_mesh_is_alive(manager, asset))
  {
    return;
  }

  x_hpool_free(&manager->pool, asset.h);
}

LDKAssetMeshData* ldk_asset_manager_mesh_get(LDKAssetManager* manager, LDKAssetMesh asset)
{
  LDKAssetHandle generic = { asset.h };
  LDKAssetInfo* info = ldk_asset_get_info(manager, generic);

  if (!info)
  {
    return NULL;
  }

  if (info->type != LDK_ASSET_TYPE_MESH)
  {
    return NULL;
  }

  return (LDKAssetMeshData*)info->data;
}

const LDKAssetMeshData* ldk_asset_manager_mesh_get_const(LDKAssetManager* manager, LDKAssetMesh asset)
{
  return ldk_asset_manager_mesh_get(manager, asset);
}
