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

typedef struct LDKParsedMesh
{
  char name[LDK_MESH_NAME_CAPACITY];
  LDKMeshVertex* vertices;
  u32 vertex_count;
  u32 vertex_written;
  u32* indices;
  u32 index_count;
  u32 index_written;
  LDKMeshMaterialSlot* material_slots;
  u32 material_slot_count;
  u32 material_slot_written;
  LDKMeshSubmesh* submeshes;
  u32 submesh_count;
  u32 submesh_written;
} LDKParsedMesh;

typedef struct LDKParsedMeshFile
{
  LDKParsedMesh* meshes;
  u32 mesh_count;
  u32 mesh_written;
} LDKParsedMeshFile;

typedef struct LDKSharedMeshLookup
{
  XFSPath path;
  LDKAssetMesh mesh;
} LDKSharedMeshLookup;

typedef struct LDKMeshSiblingLookup
{
  XFSPath path;
  const char* name;
  u32 index;
  bool find_name;
  LDKAssetMesh mesh;
} LDKMeshSiblingLookup;

static void s_mesh_asset_error(LDKMeshAssetResult* result,
    const char* message)
{
  if (result)
  {
    snprintf(result->error, sizeof(result->error), "%s",
        message ? message : "");
  }
}

static void s_mesh_asset_error_line(LDKMeshAssetResult* result,
    u32 line, const char* message)
{
  if (result)
  {
    snprintf(result->error, sizeof(result->error),
        "mesh parse error at line %u: %s", line,
        message ? message : "");
  }
}

static char* s_mesh_file_next_token(char** cursor)
{
  char* read;
  char* write;
  char* start;

  if (!cursor || !*cursor)
  {
    return NULL;
  }

  read = *cursor;
  while (*read == ' ' || *read == '\t' || *read == '\r')
  {
    read++;
  }

  if (*read == 0 || *read == '#')
  {
    *cursor = read;
    return NULL;
  }

  if (*read != '"')
  {
    start = read;
    while (*read != 0 && *read != ' ' && *read != '\t' &&
        *read != '\r' && *read != '#')
    {
      read++;
    }

    if (*read != 0)
    {
      if (*read == '#')
      {
        *read = 0;
        *cursor = read;
      }
      else
      {
        *read = 0;
        *cursor = read + 1;
      }
    }
    else
    {
      *cursor = read;
    }

    return start;
  }

  read++;
  start = read;
  write = read;

  while (*read && *read != '"')
  {
    if (*read == '\\')
    {
      read++;
      if (!*read)
      {
        return NULL;
      }

      switch (*read)
      {
        case 'n':
          *write++ = '\n';
          break;
        case 'r':
          *write++ = '\r';
          break;
        case 't':
          *write++ = '\t';
          break;
        case '\\':
          *write++ = '\\';
          break;
        case '"':
          *write++ = '"';
          break;
        default:
          return NULL;
      }
      read++;
      continue;
    }

    *write++ = *read++;
  }

  if (*read != '"')
  {
    return NULL;
  }

  *write = 0;
  read++;
  *cursor = read;
  return start;
}

static bool s_mesh_file_parse_u32(
    const char* text, int base, u32* out_value)
{
  char* end = NULL;
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

static bool s_mesh_file_parse_float(
    const char* text, float* out_value)
{
  char* end = NULL;
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

static bool s_mesh_file_parse_vertex(const char* first_token,
    char* cursor, LDKMeshVertex* out_vertex)
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
    char* token = s_mesh_file_next_token(&cursor);
    if (!token || !s_mesh_file_parse_float(token, &values[i]))
    {
      return false;
    }
  }

  char* color_token = s_mesh_file_next_token(&cursor);
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

static void s_parsed_mesh_destroy(LDKParsedMesh* mesh)
{
  if (!mesh)
  {
    return;
  }

  free(mesh->vertices);
  free(mesh->indices);
  free(mesh->material_slots);
  free(mesh->submeshes);
  memset(mesh, 0, sizeof(*mesh));
}

static void s_parsed_file_destroy(LDKParsedMeshFile* file)
{
  if (!file)
  {
    return;
  }

  for (u32 i = 0; i < file->mesh_count; i++)
  {
    s_parsed_mesh_destroy(&file->meshes[i]);
  }

  free(file->meshes);
  memset(file, 0, sizeof(*file));
}

static bool s_parsed_mesh_validate_submeshes(
    const LDKParsedMesh* mesh)
{
  bool* covered;
  u32 triangle_count;

  if (!mesh || !mesh->indices || !mesh->submeshes ||
      mesh->index_count == 0 || mesh->index_count % 3u != 0 ||
      mesh->submesh_count == 0 || mesh->material_slot_count == 0)
  {
    return false;
  }

  triangle_count = mesh->index_count / 3u;
  covered = (bool*)calloc(triangle_count, sizeof(bool));
  if (!covered)
  {
    return false;
  }

  for (u32 i = 0; i < mesh->submesh_count; i++)
  {
    const LDKMeshSubmesh* submesh = &mesh->submeshes[i];
    u32 first_triangle;
    u32 submesh_triangles;

    if (submesh->index_count == 0 ||
        submesh->first_index % 3u != 0 ||
        submesh->index_count % 3u != 0 ||
        submesh->first_index > mesh->index_count ||
        submesh->index_count > mesh->index_count - submesh->first_index ||
        submesh->material_slot >= mesh->material_slot_count)
    {
      free(covered);
      return false;
    }

    first_triangle = submesh->first_index / 3u;
    submesh_triangles = submesh->index_count / 3u;
    for (u32 triangle = first_triangle;
        triangle < first_triangle + submesh_triangles; triangle++)
    {
      if (covered[triangle])
      {
        free(covered);
        return false;
      }
      covered[triangle] = true;
    }
  }

  for (u32 triangle = 0; triangle < triangle_count; triangle++)
  {
    if (!covered[triangle])
    {
      free(covered);
      return false;
    }
  }

  free(covered);
  return true;
}

static bool s_parsed_mesh_complete(const LDKParsedMesh* mesh)
{
  return mesh && mesh->name[0] && mesh->vertices && mesh->indices &&
      mesh->material_slots && mesh->submeshes &&
      mesh->vertex_written == mesh->vertex_count &&
      mesh->index_written == mesh->index_count &&
      mesh->material_slot_written == mesh->material_slot_count &&
      mesh->submesh_written == mesh->submesh_count &&
      s_parsed_mesh_validate_submeshes(mesh);
}

static bool s_mesh_file_parse(const char* path, LDKParsedMeshFile* out_file,
    LDKMeshAssetResult* result)
{
  char* text;
  char* line;
  char* next_line;
  LDKParsedMesh* current = NULL;
  u32 line_number = 0;
  bool version_seen = false;
  bool mesh_count_seen = false;
  LDKMeshFileVertexFormat format = LDK_MESH_FILE_VERTEX_FORMAT_NONE;

  if (!path || !out_file)
  {
    s_mesh_asset_error(result, "invalid mesh load arguments");
    return false;
  }

  memset(out_file, 0, sizeof(*out_file));
  text = x_io_read_text(path, NULL);
  if (!text)
  {
    s_mesh_asset_error(result, "cannot read mesh file");
    return false;
  }

  line = text;
  while (line)
  {
    char* cursor;
    char* lhs;
    char* rhs;

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

    if (strcmp(lhs, "end_mesh") == 0)
    {
      if (!current || s_mesh_file_next_token(&cursor) != NULL ||
          !s_parsed_mesh_complete(current))
      {
        s_mesh_asset_error_line(
            result, line_number, "incomplete or invalid mesh");
        goto error;
      }
      current = NULL;
      line = next_line;
      continue;
    }

    rhs = s_mesh_file_next_token(&cursor);
    if (!rhs)
    {
      s_mesh_asset_error_line(result, line_number, "missing value");
      goto error;
    }

    if (!current)
    {
      if (strcmp(lhs, "version") == 0)
      {
        if (version_seen || strcmp(rhs, "3.0") != 0 ||
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
      else if (strcmp(lhs, "mesh_count") == 0)
      {
        u32 count;
        if (mesh_count_seen || !s_mesh_file_parse_u32(rhs, 10, &count) ||
            count == 0 || s_mesh_file_next_token(&cursor) != NULL)
        {
          s_mesh_asset_error_line(result, line_number, "invalid mesh count");
          goto error;
        }

        out_file->meshes =
            (LDKParsedMesh*)calloc(count, sizeof(LDKParsedMesh));
        if (!out_file->meshes)
        {
          s_mesh_asset_error(result, "failed to allocate mesh list");
          goto error;
        }
        out_file->mesh_count = count;
        mesh_count_seen = true;
      }
      else if (strcmp(lhs, "mesh") == 0)
      {
        if (!version_seen || format != LDK_MESH_FILE_VERTEX_FORMAT_STATIC ||
            !mesh_count_seen || out_file->mesh_written >= out_file->mesh_count ||
            !rhs[0] || strlen(rhs) >= LDK_MESH_NAME_CAPACITY ||
            s_mesh_file_next_token(&cursor) != NULL)
        {
          s_mesh_asset_error_line(result, line_number, "invalid mesh header");
          goto error;
        }

        for (u32 i = 0; i < out_file->mesh_written; i++)
        {
          if (strcmp(out_file->meshes[i].name, rhs) == 0)
          {
            s_mesh_asset_error_line(
                result, line_number, "duplicate mesh name");
            goto error;
          }
        }

        current = &out_file->meshes[out_file->mesh_written++];
        snprintf(current->name, sizeof(current->name), "%s", rhs);
      }
      else
      {
        s_mesh_asset_error_line(result, line_number, "unknown mesh entry");
        goto error;
      }

      line = next_line;
      continue;
    }

    if (strcmp(lhs, "vertex_count") == 0)
    {
      u32 count;
      if (current->vertices || !s_mesh_file_parse_u32(rhs, 10, &count) ||
          count == 0 || s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number, "invalid vertex count");
        goto error;
      }
      current->vertices =
          (LDKMeshVertex*)calloc(count, sizeof(LDKMeshVertex));
      if (!current->vertices)
      {
        s_mesh_asset_error(result, "failed to allocate mesh vertices");
        goto error;
      }
      current->vertex_count = count;
    }
    else if (strcmp(lhs, "index_count") == 0)
    {
      u32 count;
      if (current->indices || !s_mesh_file_parse_u32(rhs, 10, &count) ||
          count == 0 || count % 3u != 0 ||
          s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number, "invalid index count");
        goto error;
      }
      current->indices = (u32*)calloc(count, sizeof(u32));
      if (!current->indices)
      {
        s_mesh_asset_error(result, "failed to allocate mesh indices");
        goto error;
      }
      current->index_count = count;
    }
    else if (strcmp(lhs, "material_slot_count") == 0)
    {
      u32 count;
      if (current->material_slots ||
          !s_mesh_file_parse_u32(rhs, 10, &count) || count == 0 ||
          s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(
            result, line_number, "invalid material slot count");
        goto error;
      }
      current->material_slots = (LDKMeshMaterialSlot*)calloc(
          count, sizeof(LDKMeshMaterialSlot));
      if (!current->material_slots)
      {
        s_mesh_asset_error(result, "failed to allocate material slots");
        goto error;
      }
      current->material_slot_count = count;
    }
    else if (strcmp(lhs, "submesh_count") == 0)
    {
      u32 count;
      if (current->submeshes || !s_mesh_file_parse_u32(rhs, 10, &count) ||
          count == 0 || s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number, "invalid submesh count");
        goto error;
      }
      current->submeshes =
          (LDKMeshSubmesh*)calloc(count, sizeof(LDKMeshSubmesh));
      if (!current->submeshes)
      {
        s_mesh_asset_error(result, "failed to allocate submeshes");
        goto error;
      }
      current->submesh_count = count;
    }
    else if (strcmp(lhs, "material_slot") == 0)
    {
      u32 slot;
      char* name = s_mesh_file_next_token(&cursor);
      if (!current->material_slots || !name ||
          !s_mesh_file_parse_u32(rhs, 10, &slot) ||
          slot != current->material_slot_written ||
          slot >= current->material_slot_count ||
          strlen(name) >= LDK_MESH_MATERIAL_SLOT_NAME_CAPACITY ||
          s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number, "invalid material slot");
        goto error;
      }
      snprintf(current->material_slots[slot].name,
          sizeof(current->material_slots[slot].name), "%s", name);
      current->material_slot_written++;
    }
    else if (strcmp(lhs, "vertex") == 0)
    {
      if (!current->vertices ||
          current->vertex_written >= current->vertex_count ||
          !s_mesh_file_parse_vertex(rhs, cursor,
              &current->vertices[current->vertex_written]))
      {
        s_mesh_asset_error_line(result, line_number, "invalid vertex");
        goto error;
      }
      current->vertex_written++;
    }
    else if (strcmp(lhs, "index_list") == 0)
    {
      char* token = rhs;
      if (!current->indices || !current->vertices)
      {
        s_mesh_asset_error_line(result, line_number, "unexpected index list");
        goto error;
      }

      while (token)
      {
        u32 index;
        if (current->index_written >= current->index_count ||
            !s_mesh_file_parse_u32(token, 10, &index) ||
            index >= current->vertex_count)
        {
          s_mesh_asset_error_line(result, line_number, "invalid mesh index");
          goto error;
        }
        current->indices[current->index_written++] = index;
        token = s_mesh_file_next_token(&cursor);
      }
    }
    else if (strcmp(lhs, "submesh") == 0)
    {
      u32 first_index;
      u32 index_count;
      u32 material_slot;
      char* index_count_token = s_mesh_file_next_token(&cursor);
      char* material_slot_token = s_mesh_file_next_token(&cursor);

      if (!current->submeshes || !index_count_token ||
          !material_slot_token ||
          current->submesh_written >= current->submesh_count ||
          !s_mesh_file_parse_u32(rhs, 10, &first_index) ||
          !s_mesh_file_parse_u32(index_count_token, 10, &index_count) ||
          !s_mesh_file_parse_u32(material_slot_token, 10, &material_slot) ||
          s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number, "invalid submesh");
        goto error;
      }

      current->submeshes[current->submesh_written].first_index = first_index;
      current->submeshes[current->submesh_written].index_count = index_count;
      current->submeshes[current->submesh_written].material_slot =
          material_slot;
      current->submesh_written++;
    }
    else
    {
      s_mesh_asset_error_line(result, line_number, "unknown mesh entry");
      goto error;
    }

    line = next_line;
  }

  if (current || !version_seen ||
      format != LDK_MESH_FILE_VERTEX_FORMAT_STATIC || !mesh_count_seen ||
      out_file->mesh_written != out_file->mesh_count)
  {
    s_mesh_asset_error(result, "incomplete or inconsistent mesh file");
    goto error;
  }

  free(text);
  return true;

error:
  free(text);
  s_parsed_file_destroy(out_file);
  return false;
}

static bool s_mesh_asset_attach_metadata(LDKAssetMeshData* data,
    const LDKParsedMesh* parsed, u32 source_index, u32 source_count)
{
  size_t vertex_bytes;
  size_t submesh_bytes;
  size_t material_bytes;
  size_t total_bytes;
  u8* storage;
  u8* cursor;

  if (!data || !parsed || !data->mesh.vertices ||
      data->mesh.vertex_count != parsed->vertex_count)
  {
    return false;
  }

  vertex_bytes = (size_t)parsed->vertex_count * sizeof(LDKMeshVertex);
  submesh_bytes = (size_t)parsed->submesh_count * sizeof(LDKMeshSubmesh);
  material_bytes = (size_t)parsed->material_slot_count *
      sizeof(LDKMeshMaterialSlot);

  if (vertex_bytes > SIZE_MAX - submesh_bytes ||
      vertex_bytes + submesh_bytes > SIZE_MAX - material_bytes)
  {
    return false;
  }
  total_bytes = vertex_bytes + submesh_bytes + material_bytes;

  storage = (u8*)malloc(total_bytes);
  if (!storage)
  {
    return false;
  }

  memcpy(storage, data->mesh.vertices, vertex_bytes);
  free(data->mesh.vertices);
  data->mesh.vertices = (LDKMeshVertex*)storage;

  cursor = storage + vertex_bytes;
  data->submeshes = (LDKMeshSubmesh*)cursor;
  memcpy(data->submeshes, parsed->submeshes, submesh_bytes);
  cursor += submesh_bytes;

  data->material_slots = (LDKMeshMaterialSlot*)cursor;
  memcpy(data->material_slots, parsed->material_slots, material_bytes);

  data->submesh_count = parsed->submesh_count;
  data->material_slot_count = parsed->material_slot_count;
  data->source_mesh_index = source_index;
  data->source_mesh_count = source_count;
  snprintf(data->name, sizeof(data->name), "%s", parsed->name);
  return true;
}

static bool s_shared_mesh_find(
    LDKAssetHandle asset, LDKAssetInfo* info, void* user)
{
  LDKSharedMeshLookup* lookup = (LDKSharedMeshLookup*)user;
  LDKAssetMeshData* data;

  if (info->type != LDK_ASSET_TYPE_MESH ||
      strcmp(info->asset_path.buf, lookup->path.buf) != 0)
  {
    return true;
  }

  data = (LDKAssetMeshData*)info->data;
  if (data && data->source_mesh_index == 0)
  {
    lookup->mesh.h = asset.h;
    return false;
  }

  return true;
}

static bool s_mesh_sibling_find(
    LDKAssetHandle asset, LDKAssetInfo* info, void* user)
{
  LDKMeshSiblingLookup* lookup = (LDKMeshSiblingLookup*)user;
  LDKAssetMeshData* data;

  if (info->type != LDK_ASSET_TYPE_MESH ||
      strcmp(info->asset_path.buf, lookup->path.buf) != 0)
  {
    return true;
  }

  data = (LDKAssetMeshData*)info->data;
  if (!data)
  {
    return true;
  }

  if ((lookup->find_name && lookup->name &&
          strcmp(data->name, lookup->name) == 0) ||
      (!lookup->find_name && data->source_mesh_index == lookup->index))
  {
    lookup->mesh.h = asset.h;
    return false;
  }

  return true;
}

LDKAssetMesh ldk_asset_manager_mesh_load_shared(LDKAssetManager* manager,
    const char* path, LDKMeshAssetResult* result)
{
  LDKSharedMeshLookup lookup = {0};
  LDKParsedMeshFile parsed = {0};
  LDKAssetMesh* created = NULL;

  s_mesh_asset_error(result, "");
  lookup.mesh = ldk_asset_mesh_null();

  if (!manager || !path || !x_fs_path_is_absolute_cstr(path) ||
      strlen(path) >= sizeof(lookup.path.buf))
  {
    s_mesh_asset_error(result, "mesh asset requires an absolute file path");
    return lookup.mesh;
  }

  x_fs_path_set(&lookup.path, path);
  x_fs_path_normalize(&lookup.path);
  ldk_asset_foreach(manager, s_shared_mesh_find, &lookup);
  if (!x_handle_is_null(lookup.mesh.h))
  {
    return lookup.mesh;
  }

  if (!s_mesh_file_parse(lookup.path.buf, &parsed, result))
  {
    return lookup.mesh;
  }

  created = (LDKAssetMesh*)calloc(parsed.mesh_count, sizeof(LDKAssetMesh));
  if (!created)
  {
    s_mesh_asset_error(result, "failed to allocate mesh asset list");
    s_parsed_file_destroy(&parsed);
    return lookup.mesh;
  }

  for (u32 i = 0; i < parsed.mesh_count; i++)
  {
    const LDKParsedMesh* source = &parsed.meshes[i];
    LDKAssetHandle generic;
    LDKAssetInfo* info;
    LDKAssetMeshData* data;

    created[i] = ldk_asset_manager_mesh_create(manager, source->vertices,
        source->vertex_count, source->indices, source->index_count);
    if (x_handle_is_null(created[i].h))
    {
      s_mesh_asset_error(result, "failed to allocate mesh asset");
      goto create_error;
    }

    data = ldk_asset_manager_mesh_get(manager, created[i]);
    if (!data || !s_mesh_asset_attach_metadata(
                     data, source, i, parsed.mesh_count))
    {
      s_mesh_asset_error(result, "failed to initialize mesh metadata");
      goto create_error;
    }

    generic.h = created[i].h;
    info = ldk_asset_get_info(manager, generic);
    if (!info)
    {
      s_mesh_asset_error(result, "failed to initialize mesh asset");
      goto create_error;
    }

    info->asset_path = lookup.path;
    info->load_timestamp = (u64)time(NULL);
  }

  lookup.mesh = created[0];
  free(created);
  s_parsed_file_destroy(&parsed);
  return lookup.mesh;

create_error:
  for (u32 i = 0; i < parsed.mesh_count; i++)
  {
    if (!x_handle_is_null(created[i].h))
    {
      ldk_asset_manager_mesh_unload(manager, created[i]);
    }
  }
  free(created);
  s_parsed_file_destroy(&parsed);
  return lookup.mesh;
}

u32 ldk_asset_manager_mesh_file_mesh_count(
    LDKAssetManager* manager, LDKAssetMesh mesh)
{
  const LDKAssetMeshData* data =
      ldk_asset_manager_mesh_get_const(manager, mesh);
  return data && data->source_mesh_count ? data->source_mesh_count :
      (data ? 1u : 0u);
}

LDKAssetMesh ldk_asset_manager_mesh_file_mesh_at(
    LDKAssetManager* manager, LDKAssetMesh mesh, u32 index)
{
  LDKAssetMesh result = ldk_asset_mesh_null();
  const LDKAssetMeshData* data;
  const LDKAssetInfo* info;
  LDKAssetHandle generic;
  LDKMeshSiblingLookup lookup = {0};
  u32 count;

  if (!manager)
  {
    return result;
  }

  data = ldk_asset_manager_mesh_get_const(manager, mesh);
  count = data && data->source_mesh_count ? data->source_mesh_count :
      (data ? 1u : 0u);
  if (!data || index >= count)
  {
    return result;
  }

  if (count == 1)
  {
    return index == 0 ? mesh : result;
  }

  generic.h = mesh.h;
  info = ldk_asset_get_info_const(manager, generic);
  if (!info)
  {
    return result;
  }

  lookup.path = info->asset_path;
  lookup.index = index;
  lookup.mesh = result;
  ldk_asset_foreach(manager, s_mesh_sibling_find, &lookup);
  return lookup.mesh;
}

LDKAssetMesh ldk_asset_manager_mesh_file_mesh_find(
    LDKAssetManager* manager, LDKAssetMesh mesh, const char* name)
{
  LDKAssetMesh result = ldk_asset_mesh_null();
  const LDKAssetMeshData* data;
  const LDKAssetInfo* info;
  LDKAssetHandle generic;
  LDKMeshSiblingLookup lookup = {0};

  if (!manager || !name || !name[0])
  {
    return result;
  }

  data = ldk_asset_manager_mesh_get_const(manager, mesh);
  if (!data)
  {
    return result;
  }

  if ((!data->source_mesh_count || data->source_mesh_count == 1) &&
      strcmp(data->name, name) == 0)
  {
    return mesh;
  }

  generic.h = mesh.h;
  info = ldk_asset_get_info_const(manager, generic);
  if (!info)
  {
    return result;
  }

  lookup.path = info->asset_path;
  lookup.name = name;
  lookup.find_name = true;
  lookup.mesh = result;
  ldk_asset_foreach(manager, s_mesh_sibling_find, &lookup);
  return lookup.mesh;
}
