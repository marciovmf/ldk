#include <ldk_mesh_asset.h>
#include <stdx/stdx_io.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum LDKMeshFileVertexFormat
{
  LDK_MESH_FILE_VERTEX_FORMAT_NONE = 0,
  LDK_MESH_FILE_VERTEX_FORMAT_STATIC
} LDKMeshFileVertexFormat;

typedef struct LDKSharedMeshLookup
{
  XFSPath path;
  LDKAssetMesh mesh;
} LDKSharedMeshLookup;

static void s_mesh_asset_error(LDKMeshAssetResult *result, const char *message)
{
  if (result)
  {
    snprintf(result->error, sizeof(result->error), "%s",
        message ? message : "");
  }
}

static void s_mesh_asset_error_line(
    LDKMeshAssetResult *result, u32 line, const char *message)
{
  if (result)
  {
    snprintf(result->error, sizeof(result->error),
        "mesh parse error at line %u: %s", line, message ? message : "");
  }
}

static char *s_mesh_file_next_token(char **cursor)
{
  char *start;
  char *end;

  if (!cursor || !*cursor)
  {
    return NULL;
  }

  start = *cursor;
  while (*start == ' ' || *start == '\t' || *start == '\r')
  {
    start++;
  }

  if (*start == 0 || *start == '#')
  {
    *cursor = start;
    return NULL;
  }

  end = start;
  while (*end != 0 && *end != ' ' && *end != '\t' && *end != '\r' &&
      *end != '#')
  {
    end++;
  }

  if (*end != 0)
  {
    if (*end == '#')
    {
      *end = 0;
      *cursor = end;
    }
    else
    {
      *end = 0;
      *cursor = end + 1;
    }
  }
  else
  {
    *cursor = end;
  }

  return start;
}

static bool s_mesh_file_parse_u32(const char *text, int base, u32 *out_value)
{
  char *end = NULL;
  unsigned long value;

  if (!text || !out_value || text[0] == '-')
  {
    return false;
  }

  errno = 0;
  value = strtoul(text, &end, base);
  if (end == text || *end != 0 || errno == ERANGE || value > UINT32_MAX)
  {
    return false;
  }

  *out_value = (u32)value;
  return true;
}

static bool s_mesh_file_parse_float(const char *text, float *out_value)
{
  char *end = NULL;
  float value;

  if (!text || !out_value)
  {
    return false;
  }

  errno = 0;
  value = strtof(text, &end);
  if (end == text || *end != 0 || errno == ERANGE)
  {
    return false;
  }

  *out_value = value;
  return true;
}

static bool s_mesh_file_parse_vertex(
    const char *first_token, char *cursor, LDKMeshVertex *out_vertex)
{
  float values[8];
  u32 color;

  if (!first_token || !out_vertex ||
      !s_mesh_file_parse_float(first_token, &values[0]))
  {
    return false;
  }

  for (u32 i = 1; i < 8u; i++)
  {
    char *token = s_mesh_file_next_token(&cursor);
    if (!token || !s_mesh_file_parse_float(token, &values[i]))
    {
      return false;
    }
  }

  char *color_token = s_mesh_file_next_token(&cursor);
  if (!color_token || !s_mesh_file_parse_u32(color_token, 0, &color) ||
      s_mesh_file_next_token(&cursor) != NULL)
  {
    return false;
  }

  out_vertex->position.x = values[0];
  out_vertex->position.y = values[1];
  out_vertex->position.z = values[2];
  out_vertex->normal.x = values[3];
  out_vertex->normal.y = values[4];
  out_vertex->normal.z = values[5];
  out_vertex->uv.x = values[6];
  out_vertex->uv.y = values[7];
  out_vertex->color = LDK_RGBA32(color);
  return true;
}

static bool s_mesh_file_parse(const char *path, LDKMeshData *out_mesh,
    LDKMeshAssetResult *result)
{
  char *text;
  char *line;
  char *next_line;
  u32 line_number = 0;
  u32 vertex_written = 0;
  u32 index_written = 0;
  bool version_seen = false;
  LDKMeshFileVertexFormat format = LDK_MESH_FILE_VERTEX_FORMAT_NONE;

  if (!path || !out_mesh)
  {
    s_mesh_asset_error(result, "invalid mesh load arguments");
    return false;
  }

  memset(out_mesh, 0, sizeof(*out_mesh));
  text = x_io_read_text(path, NULL);
  if (!text)
  {
    s_mesh_asset_error(result, "cannot read mesh file");
    return false;
  }

  line = text;
  while (line)
  {
    char *cursor;
    char *lhs;
    char *rhs;

    line_number++;
    next_line = strchr(line, '\n');
    if (next_line)
    {
      *next_line = 0;
      next_line++;
    }

    cursor = line;
    lhs = s_mesh_file_next_token(&cursor);
    if (!lhs)
    {
      line = next_line;
      continue;
    }

    rhs = s_mesh_file_next_token(&cursor);
    if (!rhs)
    {
      s_mesh_asset_error_line(result, line_number, "missing value");
      goto error;
    }

    if (strcmp(lhs, "version") == 0)
    {
      if (version_seen || strcmp(rhs, "2.0") != 0 ||
          s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number,
            version_seen ? "duplicate version" : "unsupported version");
        goto error;
      }

      version_seen = true;
    }
    else if (strcmp(lhs, "vertex_format") == 0)
    {
      if (format != LDK_MESH_FILE_VERTEX_FORMAT_NONE ||
          strcmp(rhs, "STATIC") != 0 ||
          s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number,
            format != LDK_MESH_FILE_VERTEX_FORMAT_NONE
                ? "duplicate vertex format"
                : "unsupported vertex format");
        goto error;
      }

      format = LDK_MESH_FILE_VERTEX_FORMAT_STATIC;
    }
    else if (strcmp(lhs, "vertex_count") == 0)
    {
      u32 count;
      if (out_mesh->vertices || !s_mesh_file_parse_u32(rhs, 10, &count) ||
          count == 0 || s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number, "invalid vertex count");
        goto error;
      }

      out_mesh->vertices = calloc((size_t)count, sizeof(LDKMeshVertex));
      if (!out_mesh->vertices)
      {
        s_mesh_asset_error(result, "failed to allocate mesh vertices");
        goto error;
      }
      out_mesh->vertex_count = count;
    }
    else if (strcmp(lhs, "index_count") == 0)
    {
      u32 count;
      if (out_mesh->indices || !s_mesh_file_parse_u32(rhs, 10, &count) ||
          count == 0 || count % 3u != 0 ||
          s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number, "invalid index count");
        goto error;
      }

      out_mesh->indices = calloc((size_t)count, sizeof(u32));
      if (!out_mesh->indices)
      {
        s_mesh_asset_error(result, "failed to allocate mesh indices");
        goto error;
      }
      out_mesh->index_count = count;
    }
    else if (strcmp(lhs, "vertex") == 0)
    {
      if (!version_seen || format != LDK_MESH_FILE_VERTEX_FORMAT_STATIC ||
          !out_mesh->vertices || vertex_written >= out_mesh->vertex_count)
      {
        s_mesh_asset_error_line(result, line_number, "unexpected vertex");
        goto error;
      }

      if (!s_mesh_file_parse_vertex(
              rhs, cursor, &out_mesh->vertices[vertex_written]))
      {
        s_mesh_asset_error_line(result, line_number, "invalid vertex");
        goto error;
      }
      vertex_written++;
    }
    else if (strcmp(lhs, "index_list") == 0)
    {
      char *token = rhs;
      if (!version_seen || format != LDK_MESH_FILE_VERTEX_FORMAT_STATIC ||
          !out_mesh->indices || !out_mesh->vertices)
      {
        s_mesh_asset_error_line(result, line_number, "unexpected index list");
        goto error;
      }

      while (token)
      {
        u32 index;
        if (index_written >= out_mesh->index_count ||
            !s_mesh_file_parse_u32(token, 10, &index) ||
            index >= out_mesh->vertex_count)
        {
          s_mesh_asset_error_line(result, line_number, "invalid mesh index");
          goto error;
        }
        out_mesh->indices[index_written++] = index;
        token = s_mesh_file_next_token(&cursor);
      }
    }
    else
    {
      s_mesh_asset_error_line(result, line_number, "unknown mesh entry");
      goto error;
    }

    line = next_line;
  }

  if (!version_seen || format != LDK_MESH_FILE_VERTEX_FORMAT_STATIC ||
      !out_mesh->vertices || !out_mesh->indices ||
      vertex_written != out_mesh->vertex_count ||
      index_written != out_mesh->index_count)
  {
    s_mesh_asset_error(result, "incomplete or inconsistent mesh file");
    goto error;
  }

  free(text);
  return true;

error:
  free(text);
  free(out_mesh->vertices);
  free(out_mesh->indices);
  memset(out_mesh, 0, sizeof(*out_mesh));
  return false;
}

static bool s_shared_mesh_find(
    LDKAssetHandle asset, LDKAssetInfo *info, void *user)
{
  LDKSharedMeshLookup *lookup = user;

  if (info->type == LDK_ASSET_TYPE_MESH &&
      strcmp(info->asset_path.buf, lookup->path.buf) == 0)
  {
    lookup->mesh.h = asset.h;
    return false;
  }

  return true;
}

LDKAssetMesh ldk_asset_manager_mesh_load_shared(LDKAssetManager *manager,
    const char *path, LDKMeshAssetResult *result)
{
  LDKSharedMeshLookup lookup = {0};
  LDKMeshData mesh = {0};

  s_mesh_asset_error(result, "");
  lookup.mesh = ldk_asset_mesh_null();

  if (!manager || !path || !x_fs_path_is_absolute_cstr(path) ||
      strlen(path) >= sizeof(lookup.path.buf))
  {
    s_mesh_asset_error(
        result, "mesh asset requires an absolute file path");
    return lookup.mesh;
  }

  x_fs_path_set(&lookup.path, path);
  x_fs_path_normalize(&lookup.path);
  ldk_asset_foreach(manager, s_shared_mesh_find, &lookup);
  if (!x_handle_is_null(lookup.mesh.h))
  {
    return lookup.mesh;
  }

  if (!s_mesh_file_parse(lookup.path.buf, &mesh, result))
  {
    return lookup.mesh;
  }

  lookup.mesh = ldk_asset_manager_mesh_create(manager,
      mesh.vertices, mesh.vertex_count, mesh.indices, mesh.index_count);
  free(mesh.vertices);
  free(mesh.indices);

  if (x_handle_is_null(lookup.mesh.h))
  {
    s_mesh_asset_error(result, "failed to allocate mesh asset");
    return lookup.mesh;
  }

  LDKAssetHandle generic = {lookup.mesh.h};
  LDKAssetInfo *info = ldk_asset_get_info(manager, generic);
  if (!info)
  {
    ldk_asset_manager_mesh_unload(manager, lookup.mesh);
    lookup.mesh = ldk_asset_mesh_null();
    s_mesh_asset_error(result, "failed to initialize mesh asset");
    return lookup.mesh;
  }

  info->asset_path = lookup.path;
  info->load_timestamp = (u64)time(NULL);
  return lookup.mesh;
}
