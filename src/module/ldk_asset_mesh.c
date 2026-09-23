#include <ldk_mesh_asset.h>
#include <stdx/stdx_io.h>

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum LDKMeshFileVertexFormat
{
  LDK_MESH_FILE_VERTEX_FORMAT_NONE = 0,
  LDK_MESH_FILE_VERTEX_FORMAT_STATIC,
  LDK_MESH_FILE_VERTEX_FORMAT_STATIC_TANGENT
} LDKMeshFileVertexFormat;

typedef struct LDKParsedMesh
{
  char name[LDK_MESH_NAME_CAPACITY];
  LDKMeshVertex *vertices;
  u32 vertex_count;
  u32 vertex_written;
  u32 *indices;
  u32 index_count;
  u32 index_written;
  LDKMeshMaterialSlot *material_slots;
  u32 material_slot_count;
  u32 material_slot_written;
  LDKMeshSubmesh *submeshes;
  u32 submesh_count;
  u32 submesh_written;
  bool has_tangents;
} LDKParsedMesh;

typedef struct LDKParsedNode
{
  LDKMeshNode node;
  bool parent_seen;
  bool mesh_seen;
  bool position_seen;
  bool rotation_seen;
  bool scale_seen;
} LDKParsedNode;

typedef struct LDKParsedMeshFile
{
  LDKParsedMesh *meshes;
  u32 mesh_count;
  u32 mesh_written;
  LDKParsedNode *nodes;
  u32 node_count;
  u32 node_written;
} LDKParsedMeshFile;

typedef struct LDKSharedMeshLookup
{
  LDKAssetPath path;
  u64 source_revision;
  LDKAssetMesh mesh;
} LDKSharedMeshLookup;

static void s_mesh_asset_error(
    LDKMeshAssetResult *result, const char *message)
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
        "mesh parse error at line %u: %s", line,
        message ? message : "");
  }
}

static char *s_mesh_file_next_token(char **cursor)
{
  char *read;
  char *write;
  char *start;

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
    const char *text, int base, u32 *out_value)
{
  char *end = NULL;
  unsigned long value;

  if (!text || !out_value || text[0] == '-')
  {
    return false;
  }

  errno = 0;
  value = strtoul(text, &end, base);
  if (end == text || *end != 0 || errno == ERANGE ||
      value > UINT32_MAX)
  {
    return false;
  }

  *out_value = (u32)value;
  return true;
}

static bool s_mesh_file_parse_i32(const char *text, i32 *out_value)
{
  char *end = NULL;
  long value;

  if (!text || !out_value)
  {
    return false;
  }

  errno = 0;
  value = strtol(text, &end, 10);
  if (end == text || *end != 0 || errno == ERANGE ||
      value < INT32_MIN || value > INT32_MAX)
  {
    return false;
  }

  *out_value = (i32)value;
  return true;
}

static bool s_mesh_file_parse_float(
    const char *text, float *out_value)
{
  char *end = NULL;
  float value;

  if (!text || !out_value)
  {
    return false;
  }

  errno = 0;
  value = strtof(text, &end);
  if (end == text || *end != 0 || errno == ERANGE || !isfinite(value))
  {
    return false;
  }

  *out_value = value;
  return true;
}

static bool s_mesh_file_parse_vec3(
    const char *first, char *cursor, Vec3 *out_value)
{
  float values[3];

  if (!first || !out_value ||
      !s_mesh_file_parse_float(first, &values[0]))
  {
    return false;
  }

  for (u32 i = 1; i < 3u; ++i)
  {
    char *token = s_mesh_file_next_token(&cursor);
    if (!token || !s_mesh_file_parse_float(token, &values[i]))
    {
      return false;
    }
  }

  if (s_mesh_file_next_token(&cursor) != NULL)
  {
    return false;
  }

  *out_value = vec3_make(values[0], values[1], values[2]);
  return true;
}

static bool s_mesh_file_parse_quat(
    const char *first, char *cursor, Quat *out_value)
{
  float values[4];

  if (!first || !out_value ||
      !s_mesh_file_parse_float(first, &values[0]))
  {
    return false;
  }

  for (u32 i = 1; i < 4u; ++i)
  {
    char *token = s_mesh_file_next_token(&cursor);
    if (!token || !s_mesh_file_parse_float(token, &values[i]))
    {
      return false;
    }
  }

  if (s_mesh_file_next_token(&cursor) != NULL)
  {
    return false;
  }

  out_value->x = values[0];
  out_value->y = values[1];
  out_value->z = values[2];
  out_value->w = values[3];
  return true;
}

static bool s_mesh_file_parse_vertex(const char *first_token, char *cursor,
    LDKMeshFileVertexFormat format, LDKMeshVertex *out_vertex)
{
  float values[12] = {0};
  u32 color;
  u32 value_count =
      format == LDK_MESH_FILE_VERTEX_FORMAT_STATIC_TANGENT ? 12u : 8u;

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
  if (!color_token || !s_mesh_file_parse_u32(color_token, 0, &color))
  {
    return false;
  }

  for (u32 i = 8u; i < value_count; i++)
  {
    char *token = s_mesh_file_next_token(&cursor);
    if (!token || !s_mesh_file_parse_float(token, &values[i]))
    {
      return false;
    }
  }

  if (s_mesh_file_next_token(&cursor) != NULL)
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
  out_vertex->tangent.x = values[8];
  out_vertex->tangent.y = values[9];
  out_vertex->tangent.z = values[10];
  out_vertex->tangent.w = values[11];
  return true;
}

static void s_parsed_mesh_destroy(LDKParsedMesh *mesh)
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

static void s_parsed_file_destroy(LDKParsedMeshFile *file)
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
  free(file->nodes);
  memset(file, 0, sizeof(*file));
}

static bool s_parsed_mesh_validate_submeshes(
    const LDKParsedMesh *mesh)
{
  bool *covered;
  u32 triangle_count;

  if (!mesh || !mesh->indices || !mesh->submeshes ||
      mesh->index_count == 0 || mesh->index_count % 3u != 0 ||
      mesh->submesh_count == 0 || mesh->material_slot_count == 0)
  {
    return false;
  }

  triangle_count = mesh->index_count / 3u;
  covered = (bool *)calloc(triangle_count, sizeof(bool));
  if (!covered)
  {
    return false;
  }

  for (u32 i = 0; i < mesh->submesh_count; i++)
  {
    const LDKMeshSubmesh *submesh = &mesh->submeshes[i];
    u32 first_triangle;
    u32 submesh_triangles;

    if (submesh->index_count == 0 || submesh->first_index % 3u != 0 ||
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

static bool s_parsed_mesh_complete(const LDKParsedMesh *mesh)
{
  return mesh && mesh->name[0] && mesh->vertices && mesh->indices &&
      mesh->material_slots && mesh->submeshes &&
      mesh->vertex_written == mesh->vertex_count &&
      mesh->index_written == mesh->index_count &&
      mesh->material_slot_written == mesh->material_slot_count &&
      mesh->submesh_written == mesh->submesh_count &&
      s_parsed_mesh_validate_submeshes(mesh);
}

static bool s_parsed_node_complete(
    const LDKParsedNode *node, u32 node_index, u32 mesh_count)
{
  if (!node || !node->node.name[0] || !node->parent_seen ||
      !node->mesh_seen || !node->position_seen || !node->rotation_seen ||
      !node->scale_seen)
  {
    return false;
  }

  if (node->node.parent_index < LDK_MESH_NODE_NONE ||
      node->node.parent_index >= (i32)node_index)
  {
    return false;
  }

  return node->node.mesh_index >= LDK_MESH_INDEX_NONE &&
      node->node.mesh_index < (i32)mesh_count;
}

static bool s_mesh_file_parse(LDKAssetManager *manager, const char *path,
    LDKParsedMeshFile *out_file, LDKMeshAssetResult *result)
{
  LDKAssetSourceFile file;
  u64 size;
  char *text;
  char *line;
  char *next_line;
  LDKParsedMesh *current_mesh = NULL;
  LDKParsedNode *current_node = NULL;
  u32 current_node_index = 0;
  u32 line_number = 0;
  bool version_seen = false;
  bool version_has_tangents = false;
  bool mesh_count_seen = false;
  bool node_count_seen = false;
  LDKMeshFileVertexFormat format = LDK_MESH_FILE_VERTEX_FORMAT_NONE;

  if (!manager || !manager->source || !path || !out_file ||
      !ldk_asset_source_find(manager->source, path, &file))
  {
    s_mesh_asset_error(result, "cannot read mesh asset");
    return false;
  }

  size = ldk_asset_source_file_size(&file);
  if (size > (u64)SIZE_MAX - 1u)
  {
    s_mesh_asset_error(result, "mesh asset is too large");
    return false;
  }

  memset(out_file, 0, sizeof(*out_file));
  text = (char *)malloc((size_t)size + 1u);
  if (!text || !ldk_asset_source_file_read(&file, text, size))
  {
    free(text);
    s_mesh_asset_error(result, "cannot read mesh asset");
    return false;
  }
  text[size] = 0;

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

    if (strcmp(lhs, "end_mesh") == 0)
    {
      if (!current_mesh || current_node ||
          s_mesh_file_next_token(&cursor) != NULL ||
          !s_parsed_mesh_complete(current_mesh))
      {
        s_mesh_asset_error_line(
            result, line_number, "incomplete or invalid mesh");
        goto error;
      }
      current_mesh = NULL;
      line = next_line;
      continue;
    }

    if (strcmp(lhs, "end_node") == 0)
    {
      if (!current_node || current_mesh ||
          s_mesh_file_next_token(&cursor) != NULL ||
          !s_parsed_node_complete(
              current_node, current_node_index, out_file->mesh_count))
      {
        s_mesh_asset_error_line(
            result, line_number, "incomplete or invalid node");
        goto error;
      }
      current_node = NULL;
      line = next_line;
      continue;
    }

    rhs = s_mesh_file_next_token(&cursor);
    if (!rhs)
    {
      s_mesh_asset_error_line(result, line_number, "missing value");
      goto error;
    }

    if (!current_mesh && !current_node)
    {
      if (strcmp(lhs, "version") == 0)
      {
        if (version_seen ||
            (strcmp(rhs, "4.0") != 0 && strcmp(rhs, "4.1") != 0) ||
            s_mesh_file_next_token(&cursor) != NULL)
        {
          s_mesh_asset_error_line(result, line_number,
              version_seen ? "duplicate version" : "unsupported version");
          goto error;
        }
        version_has_tangents = strcmp(rhs, "4.1") == 0;
        version_seen = true;
      }
      else if (strcmp(lhs, "vertex_format") == 0)
      {
        if (format != LDK_MESH_FILE_VERTEX_FORMAT_NONE ||
            (strcmp(rhs, "STATIC") != 0 &&
                strcmp(rhs, "STATIC_TANGENT") != 0) ||
            s_mesh_file_next_token(&cursor) != NULL)
        {
          s_mesh_asset_error_line(result, line_number,
              format != LDK_MESH_FILE_VERTEX_FORMAT_NONE
                  ? "duplicate vertex format"
                  : "unsupported vertex format");
          goto error;
        }
        format = strcmp(rhs, "STATIC_TANGENT") == 0
                     ? LDK_MESH_FILE_VERTEX_FORMAT_STATIC_TANGENT
                     : LDK_MESH_FILE_VERTEX_FORMAT_STATIC;
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
            (LDKParsedMesh *)calloc(count, sizeof(LDKParsedMesh));
        if (!out_file->meshes)
        {
          s_mesh_asset_error(result, "failed to allocate mesh list");
          goto error;
        }
        out_file->mesh_count = count;
        mesh_count_seen = true;
      }
      else if (strcmp(lhs, "node_count") == 0)
      {
        u32 count;
        if (node_count_seen || !s_mesh_file_parse_u32(rhs, 10, &count) ||
            s_mesh_file_next_token(&cursor) != NULL)
        {
          s_mesh_asset_error_line(result, line_number, "invalid node count");
          goto error;
        }

        if (count)
        {
          out_file->nodes =
              (LDKParsedNode *)calloc(count, sizeof(LDKParsedNode));
          if (!out_file->nodes)
          {
            s_mesh_asset_error(result, "failed to allocate node list");
            goto error;
          }
        }
        out_file->node_count = count;
        node_count_seen = true;
      }
      else if (strcmp(lhs, "mesh") == 0)
      {
        u32 index;
        char *name = s_mesh_file_next_token(&cursor);
        bool format_matches_version =
            (!version_has_tangents &&
                format == LDK_MESH_FILE_VERTEX_FORMAT_STATIC) ||
            (version_has_tangents &&
                format == LDK_MESH_FILE_VERTEX_FORMAT_STATIC_TANGENT);
        if (!version_seen || !format_matches_version || !mesh_count_seen ||
            !name || !name[0] || !s_mesh_file_parse_u32(rhs, 10, &index) ||
            index != out_file->mesh_written || index >= out_file->mesh_count ||
            strlen(name) >= LDK_MESH_NAME_CAPACITY ||
            s_mesh_file_next_token(&cursor) != NULL)
        {
          s_mesh_asset_error_line(result, line_number, "invalid mesh header");
          goto error;
        }

        current_mesh = &out_file->meshes[out_file->mesh_written++];
        current_mesh->has_tangents =
            format == LDK_MESH_FILE_VERTEX_FORMAT_STATIC_TANGENT;
        snprintf(current_mesh->name, sizeof(current_mesh->name), "%s", name);
      }
      else if (strcmp(lhs, "node") == 0)
      {
        u32 index;
        char *name = s_mesh_file_next_token(&cursor);
        if (!version_seen || !mesh_count_seen || !node_count_seen ||
            !name || !name[0] || !s_mesh_file_parse_u32(rhs, 10, &index) ||
            index != out_file->node_written || index >= out_file->node_count ||
            strlen(name) >= LDK_MESH_NODE_NAME_CAPACITY ||
            s_mesh_file_next_token(&cursor) != NULL)
        {
          s_mesh_asset_error_line(result, line_number, "invalid node header");
          goto error;
        }

        current_node_index = index;
        current_node = &out_file->nodes[out_file->node_written++];
        current_node->node.parent_index = LDK_MESH_NODE_NONE;
        current_node->node.mesh_index = LDK_MESH_INDEX_NONE;
        snprintf(current_node->node.name, sizeof(current_node->node.name),
            "%s", name);
      }
      else
      {
        s_mesh_asset_error_line(result, line_number, "unknown mesh entry");
        goto error;
      }

      line = next_line;
      continue;
    }

    if (current_node)
    {
      if (strcmp(lhs, "parent") == 0)
      {
        if (current_node->parent_seen ||
            !s_mesh_file_parse_i32(rhs, &current_node->node.parent_index) ||
            s_mesh_file_next_token(&cursor) != NULL)
        {
          s_mesh_asset_error_line(result, line_number, "invalid node parent");
          goto error;
        }
        current_node->parent_seen = true;
      }
      else if (strcmp(lhs, "mesh") == 0)
      {
        if (current_node->mesh_seen ||
            !s_mesh_file_parse_i32(rhs, &current_node->node.mesh_index) ||
            s_mesh_file_next_token(&cursor) != NULL)
        {
          s_mesh_asset_error_line(result, line_number, "invalid node mesh");
          goto error;
        }
        current_node->mesh_seen = true;
      }
      else if (strcmp(lhs, "position") == 0)
      {
        if (current_node->position_seen ||
            !s_mesh_file_parse_vec3(
                rhs, cursor, &current_node->node.local_position))
        {
          s_mesh_asset_error_line(
              result, line_number, "invalid node position");
          goto error;
        }
        current_node->position_seen = true;
      }
      else if (strcmp(lhs, "rotation") == 0)
      {
        if (current_node->rotation_seen ||
            !s_mesh_file_parse_quat(
                rhs, cursor, &current_node->node.local_rotation))
        {
          s_mesh_asset_error_line(
              result, line_number, "invalid node rotation");
          goto error;
        }
        current_node->rotation_seen = true;
      }
      else if (strcmp(lhs, "scale") == 0)
      {
        if (current_node->scale_seen ||
            !s_mesh_file_parse_vec3(
                rhs, cursor, &current_node->node.local_scale))
        {
          s_mesh_asset_error_line(result, line_number, "invalid node scale");
          goto error;
        }
        current_node->scale_seen = true;
      }
      else
      {
        s_mesh_asset_error_line(result, line_number, "unknown node entry");
        goto error;
      }

      line = next_line;
      continue;
    }

    if (strcmp(lhs, "vertex_count") == 0)
    {
      u32 count;
      if (current_mesh->vertices ||
          !s_mesh_file_parse_u32(rhs, 10, &count) || count == 0 ||
          s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number, "invalid vertex count");
        goto error;
      }
      current_mesh->vertices =
          (LDKMeshVertex *)calloc(count, sizeof(LDKMeshVertex));
      if (!current_mesh->vertices)
      {
        s_mesh_asset_error(result, "failed to allocate mesh vertices");
        goto error;
      }
      current_mesh->vertex_count = count;
    }
    else if (strcmp(lhs, "index_count") == 0)
    {
      u32 count;
      if (current_mesh->indices ||
          !s_mesh_file_parse_u32(rhs, 10, &count) || count == 0 ||
          count % 3u != 0 || s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number, "invalid index count");
        goto error;
      }
      current_mesh->indices = (u32 *)calloc(count, sizeof(u32));
      if (!current_mesh->indices)
      {
        s_mesh_asset_error(result, "failed to allocate mesh indices");
        goto error;
      }
      current_mesh->index_count = count;
    }
    else if (strcmp(lhs, "material_slot_count") == 0)
    {
      u32 count;
      if (current_mesh->material_slots ||
          !s_mesh_file_parse_u32(rhs, 10, &count) || count == 0 ||
          s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(
            result, line_number, "invalid material slot count");
        goto error;
      }
      current_mesh->material_slots = (LDKMeshMaterialSlot *)calloc(
          count, sizeof(LDKMeshMaterialSlot));
      if (!current_mesh->material_slots)
      {
        s_mesh_asset_error(result, "failed to allocate material slots");
        goto error;
      }
      current_mesh->material_slot_count = count;
    }
    else if (strcmp(lhs, "submesh_count") == 0)
    {
      u32 count;
      if (current_mesh->submeshes ||
          !s_mesh_file_parse_u32(rhs, 10, &count) || count == 0 ||
          s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number, "invalid submesh count");
        goto error;
      }
      current_mesh->submeshes =
          (LDKMeshSubmesh *)calloc(count, sizeof(LDKMeshSubmesh));
      if (!current_mesh->submeshes)
      {
        s_mesh_asset_error(result, "failed to allocate submeshes");
        goto error;
      }
      current_mesh->submesh_count = count;
    }
    else if (strcmp(lhs, "material_slot") == 0)
    {
      u32 slot;
      char *name = s_mesh_file_next_token(&cursor);
      if (!current_mesh->material_slots || !name ||
          !s_mesh_file_parse_u32(rhs, 10, &slot) ||
          slot != current_mesh->material_slot_written ||
          slot >= current_mesh->material_slot_count ||
          strlen(name) >= LDK_MESH_MATERIAL_SLOT_NAME_CAPACITY ||
          s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number, "invalid material slot");
        goto error;
      }
      snprintf(current_mesh->material_slots[slot].name,
          sizeof(current_mesh->material_slots[slot].name), "%s", name);
      current_mesh->material_slot_written++;
    }
    else if (strcmp(lhs, "vertex") == 0)
    {
      if (!current_mesh->vertices ||
          current_mesh->vertex_written >= current_mesh->vertex_count ||
          !s_mesh_file_parse_vertex(rhs, cursor, format,
              &current_mesh->vertices[current_mesh->vertex_written]))
      {
        s_mesh_asset_error_line(result, line_number, "invalid vertex");
        goto error;
      }
      current_mesh->vertex_written++;
    }
    else if (strcmp(lhs, "index_list") == 0)
    {
      char *token = rhs;
      if (!current_mesh->indices || !current_mesh->vertices)
      {
        s_mesh_asset_error_line(result, line_number, "unexpected index list");
        goto error;
      }

      while (token)
      {
        u32 index;
        if (current_mesh->index_written >= current_mesh->index_count ||
            !s_mesh_file_parse_u32(token, 10, &index) ||
            index >= current_mesh->vertex_count)
        {
          s_mesh_asset_error_line(result, line_number, "invalid mesh index");
          goto error;
        }
        current_mesh->indices[current_mesh->index_written++] = index;
        token = s_mesh_file_next_token(&cursor);
      }
    }
    else if (strcmp(lhs, "submesh") == 0)
    {
      u32 first_index;
      u32 index_count;
      u32 material_slot;
      char *index_count_token = s_mesh_file_next_token(&cursor);
      char *material_slot_token = s_mesh_file_next_token(&cursor);

      if (!current_mesh->submeshes || !index_count_token ||
          !material_slot_token ||
          current_mesh->submesh_written >= current_mesh->submesh_count ||
          !s_mesh_file_parse_u32(rhs, 10, &first_index) ||
          !s_mesh_file_parse_u32(index_count_token, 10, &index_count) ||
          !s_mesh_file_parse_u32(
              material_slot_token, 10, &material_slot) ||
          s_mesh_file_next_token(&cursor) != NULL)
      {
        s_mesh_asset_error_line(result, line_number, "invalid submesh");
        goto error;
      }

      current_mesh->submeshes[current_mesh->submesh_written].first_index =
          first_index;
      current_mesh->submeshes[current_mesh->submesh_written].index_count =
          index_count;
      current_mesh->submeshes[current_mesh->submesh_written].material_slot =
          material_slot;
      current_mesh->submesh_written++;
    }
    else
    {
      s_mesh_asset_error_line(result, line_number, "unknown mesh entry");
      goto error;
    }

    line = next_line;
  }

  bool format_matches_version =
      (!version_has_tangents && format == LDK_MESH_FILE_VERTEX_FORMAT_STATIC) ||
      (version_has_tangents &&
          format == LDK_MESH_FILE_VERTEX_FORMAT_STATIC_TANGENT);
  if (current_mesh || current_node || !version_seen ||
      !format_matches_version || !mesh_count_seen || !node_count_seen ||
      out_file->mesh_written != out_file->mesh_count ||
      out_file->node_written != out_file->node_count)
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

static size_t s_mesh_align_size(size_t value, size_t alignment)
{
  size_t mask = alignment - 1u;
  return (value + mask) & ~mask;
}

static bool s_mesh_size_add(size_t *value, size_t addition)
{
  if (!value || addition > SIZE_MAX - *value)
  {
    return false;
  }
  *value += addition;
  return true;
}

static bool s_mesh_asset_build_storage(const LDKParsedMeshFile *parsed,
    u8 **out_vertex_storage, u32 **out_index_storage,
    size_t *out_vertex_bytes, size_t *out_index_bytes)
{
  size_t vertex_offset = 0;
  size_t entry_offset;
  size_t submesh_offset;
  size_t material_offset;
  size_t node_offset;
  size_t index_count = 0;
  size_t total_submeshes = 0;
  size_t total_material_slots = 0;
  u8 *vertex_storage;
  u32 *index_storage;
  LDKAssetMeshEntry *entries;
  LDKMeshSubmesh *submeshes;
  LDKMeshMaterialSlot *materials;
  LDKMeshNode *nodes;
  size_t vertex_cursor = 0;
  size_t index_cursor = 0;
  size_t submesh_cursor = 0;
  size_t material_cursor = 0;

  if (!parsed || !out_vertex_storage || !out_index_storage ||
      !out_vertex_bytes || !out_index_bytes || !parsed->mesh_count)
  {
    return false;
  }

  for (u32 i = 0; i < parsed->mesh_count; ++i)
  {
    const LDKParsedMesh *mesh = &parsed->meshes[i];
    if (!s_mesh_size_add(&vertex_offset,
            (size_t)mesh->vertex_count * sizeof(LDKMeshVertex)) ||
        !s_mesh_size_add(&index_count, mesh->index_count) ||
        !s_mesh_size_add(&total_submeshes, mesh->submesh_count) ||
        !s_mesh_size_add(
            &total_material_slots, mesh->material_slot_count))
    {
      return false;
    }
  }

  entry_offset = s_mesh_align_size(
      vertex_offset, _Alignof(LDKAssetMeshEntry));
  vertex_offset = entry_offset;
  if (!s_mesh_size_add(&vertex_offset,
          (size_t)parsed->mesh_count * sizeof(LDKAssetMeshEntry)))
  {
    return false;
  }

  submesh_offset = s_mesh_align_size(
      vertex_offset, _Alignof(LDKMeshSubmesh));
  vertex_offset = submesh_offset;
  if (!s_mesh_size_add(&vertex_offset,
          total_submeshes * sizeof(LDKMeshSubmesh)))
  {
    return false;
  }

  material_offset = s_mesh_align_size(
      vertex_offset, _Alignof(LDKMeshMaterialSlot));
  vertex_offset = material_offset;
  if (!s_mesh_size_add(&vertex_offset,
          total_material_slots * sizeof(LDKMeshMaterialSlot)))
  {
    return false;
  }

  node_offset = s_mesh_align_size(vertex_offset, _Alignof(LDKMeshNode));
  vertex_offset = node_offset;
  if (!s_mesh_size_add(&vertex_offset,
          (size_t)parsed->node_count * sizeof(LDKMeshNode)) ||
      index_count > SIZE_MAX / sizeof(u32))
  {
    return false;
  }

  vertex_storage = (u8 *)malloc(vertex_offset);
  index_storage = (u32 *)malloc(index_count * sizeof(u32));
  if (!vertex_storage || !index_storage)
  {
    free(vertex_storage);
    free(index_storage);
    return false;
  }

  memset(vertex_storage, 0, vertex_offset);
  entries = (LDKAssetMeshEntry *)(vertex_storage + entry_offset);
  submeshes = (LDKMeshSubmesh *)(vertex_storage + submesh_offset);
  materials = (LDKMeshMaterialSlot *)(vertex_storage + material_offset);
  nodes = parsed->node_count
      ? (LDKMeshNode *)(vertex_storage + node_offset)
      : NULL;

  for (u32 i = 0; i < parsed->mesh_count; ++i)
  {
    const LDKParsedMesh *source = &parsed->meshes[i];
    LDKAssetMeshEntry *entry = &entries[i];
    size_t vertex_bytes =
        (size_t)source->vertex_count * sizeof(LDKMeshVertex);
    size_t index_bytes = (size_t)source->index_count * sizeof(u32);

    snprintf(entry->name, sizeof(entry->name), "%s", source->name);
    entry->mesh.vertices = (LDKMeshVertex *)(vertex_storage + vertex_cursor);
    entry->mesh.vertex_count = source->vertex_count;
    entry->mesh.indices = index_storage + index_cursor;
    entry->mesh.index_count = source->index_count;
    entry->mesh.has_tangents = source->has_tangents;
    entry->submeshes = submeshes + submesh_cursor;
    entry->submesh_count = source->submesh_count;
    entry->material_slots = materials + material_cursor;
    entry->material_slot_count = source->material_slot_count;

    memcpy(entry->mesh.vertices, source->vertices, vertex_bytes);
    memcpy(entry->mesh.indices, source->indices, index_bytes);
    memcpy(entry->submeshes, source->submeshes,
        (size_t)source->submesh_count * sizeof(LDKMeshSubmesh));
    memcpy(entry->material_slots, source->material_slots,
        (size_t)source->material_slot_count * sizeof(LDKMeshMaterialSlot));

    vertex_cursor += vertex_bytes;
    index_cursor += source->index_count;
    submesh_cursor += source->submesh_count;
    material_cursor += source->material_slot_count;
  }

  for (u32 i = 0; i < parsed->node_count; ++i)
  {
    nodes[i] = parsed->nodes[i].node;
  }

  *out_vertex_storage = vertex_storage;
  *out_index_storage = index_storage;
  *out_vertex_bytes = vertex_offset;
  *out_index_bytes = index_count * sizeof(u32);
  return true;
}

static bool s_shared_mesh_find(
    LDKAssetHandle asset, LDKAssetInfo *info, void *user)
{
  LDKSharedMeshLookup *lookup = (LDKSharedMeshLookup *)user;

  if (info->type == LDK_ASSET_TYPE_MESH &&
      info->source_revision == lookup->source_revision &&
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
  LDKParsedMeshFile parsed = {0};
  LDKAssetMesh asset = ldk_asset_mesh_null();
  LDKAssetMeshData *data;
  LDKAssetHandle generic;
  LDKAssetInfo *info;
  u8 *vertex_storage = NULL;
  u32 *index_storage = NULL;
  size_t vertex_storage_bytes = 0;
  size_t index_storage_bytes = 0;
  size_t total_vertex_bytes = 0;
  size_t entry_offset;
  size_t submesh_offset;
  size_t material_offset;
  size_t node_offset;
  size_t total_submeshes = 0;
  size_t total_material_slots = 0;

  s_mesh_asset_error(result, "");
  lookup.mesh = ldk_asset_mesh_null();

  if (!manager || !manager->source ||
      !ldk_asset_path_set(&lookup.path, path))
  {
    s_mesh_asset_error(result, "mesh asset requires a valid asset path");
    return lookup.mesh;
  }

  lookup.source_revision = manager->source->revision;
  ldk_asset_foreach(manager, s_shared_mesh_find, &lookup);
  if (!x_handle_is_null(lookup.mesh.h))
  {
    return lookup.mesh;
  }

  if (!s_mesh_file_parse(manager, lookup.path.buf, &parsed, result))
  {
    return lookup.mesh;
  }

  if (!s_mesh_asset_build_storage(&parsed, &vertex_storage, &index_storage,
          &vertex_storage_bytes, &index_storage_bytes))
  {
    s_mesh_asset_error(result, "failed to allocate mesh asset storage");
    s_parsed_file_destroy(&parsed);
    return lookup.mesh;
  }

  asset = ldk_asset_manager_mesh_create(manager, parsed.meshes[0].vertices,
      parsed.meshes[0].vertex_count, parsed.meshes[0].indices,
      parsed.meshes[0].index_count);
  if (x_handle_is_null(asset.h))
  {
    s_mesh_asset_error(result, "failed to allocate mesh asset");
    free(vertex_storage);
    free(index_storage);
    s_parsed_file_destroy(&parsed);
    return lookup.mesh;
  }

  data = ldk_asset_manager_mesh_get(manager, asset);
  if (!data)
  {
    s_mesh_asset_error(result, "failed to initialize mesh asset");
    free(vertex_storage);
    free(index_storage);
    ldk_asset_manager_mesh_unload(manager, asset);
    s_parsed_file_destroy(&parsed);
    return lookup.mesh;
  }

  for (u32 i = 0; i < parsed.mesh_count; ++i)
  {
    total_vertex_bytes +=
        (size_t)parsed.meshes[i].vertex_count * sizeof(LDKMeshVertex);
    total_submeshes += parsed.meshes[i].submesh_count;
    total_material_slots += parsed.meshes[i].material_slot_count;
  }

  entry_offset = s_mesh_align_size(
      total_vertex_bytes, _Alignof(LDKAssetMeshEntry));
  submesh_offset = s_mesh_align_size(
      entry_offset + (size_t)parsed.mesh_count * sizeof(LDKAssetMeshEntry),
      _Alignof(LDKMeshSubmesh));
  material_offset = s_mesh_align_size(
      submesh_offset + total_submeshes * sizeof(LDKMeshSubmesh),
      _Alignof(LDKMeshMaterialSlot));
  node_offset = s_mesh_align_size(
      material_offset +
          total_material_slots * sizeof(LDKMeshMaterialSlot),
      _Alignof(LDKMeshNode));

  free(data->mesh.vertices);
  free(data->mesh.indices);
  data->mesh.vertices = (LDKMeshVertex *)vertex_storage;
  data->mesh.vertex_count = parsed.meshes[0].vertex_count;
  data->mesh.indices = index_storage;
  data->mesh.index_count = parsed.meshes[0].index_count;
  data->mesh.has_tangents = parsed.meshes[0].has_tangents;
  data->meshes = (LDKAssetMeshEntry *)(vertex_storage + entry_offset);
  data->mesh_count = parsed.mesh_count;
  data->nodes = parsed.node_count
      ? (LDKMeshNode *)(vertex_storage + node_offset)
      : NULL;
  data->node_count = parsed.node_count;

  generic.h = asset.h;
  info = ldk_asset_get_info(manager, generic);
  if (!info)
  {
    s_mesh_asset_error(result, "failed to initialize mesh asset metadata");
    ldk_asset_manager_mesh_unload(manager, asset);
    s_parsed_file_destroy(&parsed);
    return ldk_asset_mesh_null();
  }

  info->asset_path = lookup.path;
  info->source_revision = lookup.source_revision;
  info->load_timestamp = (u64)time(NULL);

  (void)vertex_storage_bytes;
  (void)index_storage_bytes;
  s_parsed_file_destroy(&parsed);
  return asset;
}

u32 ldk_asset_manager_mesh_count(
    LDKAssetManager *manager, LDKAssetMesh asset)
{
  const LDKAssetMeshData *data =
      ldk_asset_manager_mesh_get_const(manager, asset);

  if (!data)
  {
    return 0;
  }

  return data->mesh_count ? data->mesh_count : 1u;
}

const char *ldk_asset_manager_mesh_name(
    LDKAssetManager *manager, LDKAssetMesh asset, u32 mesh_index)
{
  const LDKAssetMeshData *data =
      ldk_asset_manager_mesh_get_const(manager, asset);

  if (!data || mesh_index >= (data->mesh_count ? data->mesh_count : 1u))
  {
    return NULL;
  }

  if (data->mesh_count)
  {
    return data->meshes[mesh_index].name;
  }

  LDKAssetHandle generic = {asset.h};
  const LDKAssetInfo *info = ldk_asset_get_info_const(manager, generic);
  if (info && info->asset_path.length)
  {
    const char *path = info->asset_path.buf;
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *name = slash;
    if (!name || (backslash && backslash > name))
    {
      name = backslash;
    }
    return name ? name + 1 : path;
  }

  return "Mesh";
}

const LDKMeshData *ldk_asset_manager_mesh_data_at(
    LDKAssetManager *manager, LDKAssetMesh asset, u32 mesh_index)
{
  const LDKAssetMeshData *data =
      ldk_asset_manager_mesh_get_const(manager, asset);

  if (!data || mesh_index >= (data->mesh_count ? data->mesh_count : 1u))
  {
    return NULL;
  }

  return data->mesh_count ? &data->meshes[mesh_index].mesh : &data->mesh;
}

u32 ldk_asset_manager_mesh_material_slot_count(
    LDKAssetManager *manager, LDKAssetMesh asset, u32 mesh_index)
{
  const LDKAssetMeshData *data =
      ldk_asset_manager_mesh_get_const(manager, asset);

  if (!data || mesh_index >= (data->mesh_count ? data->mesh_count : 1u))
  {
    return 0;
  }

  return data->mesh_count
      ? data->meshes[mesh_index].material_slot_count
      : 1u;
}

const char *ldk_asset_manager_mesh_material_slot_name(
    LDKAssetManager *manager, LDKAssetMesh asset, u32 mesh_index,
    u32 material_slot)
{
  const LDKAssetMeshData *data =
      ldk_asset_manager_mesh_get_const(manager, asset);

  if (!data || mesh_index >= (data->mesh_count ? data->mesh_count : 1u))
  {
    return NULL;
  }

  if (!data->mesh_count)
  {
    return material_slot == 0 ? "" : NULL;
  }

  const LDKAssetMeshEntry *mesh = &data->meshes[mesh_index];
  if (material_slot >= mesh->material_slot_count)
  {
    return NULL;
  }

  return mesh->material_slots[material_slot].name;
}

u32 ldk_asset_manager_mesh_submesh_count(
    LDKAssetManager *manager, LDKAssetMesh asset, u32 mesh_index)
{
  const LDKAssetMeshData *data =
      ldk_asset_manager_mesh_get_const(manager, asset);

  if (!data || mesh_index >= (data->mesh_count ? data->mesh_count : 1u))
  {
    return 0;
  }

  return data->mesh_count ? data->meshes[mesh_index].submesh_count : 0u;
}

const LDKMeshSubmesh *ldk_asset_manager_mesh_submesh_at(
    LDKAssetManager *manager, LDKAssetMesh asset, u32 mesh_index,
    u32 submesh_index)
{
  const LDKAssetMeshData *data =
      ldk_asset_manager_mesh_get_const(manager, asset);

  if (!data || !data->mesh_count || mesh_index >= data->mesh_count)
  {
    return NULL;
  }

  const LDKAssetMeshEntry *mesh = &data->meshes[mesh_index];
  return submesh_index < mesh->submesh_count
      ? &mesh->submeshes[submesh_index]
      : NULL;
}

u32 ldk_asset_manager_mesh_node_count(
    LDKAssetManager *manager, LDKAssetMesh asset)
{
  const LDKAssetMeshData *data =
      ldk_asset_manager_mesh_get_const(manager, asset);
  return data ? data->node_count : 0u;
}

const LDKMeshNode *ldk_asset_manager_mesh_node_at(
    LDKAssetManager *manager, LDKAssetMesh asset, u32 node_index)
{
  const LDKAssetMeshData *data =
      ldk_asset_manager_mesh_get_const(manager, asset);

  if (!data || node_index >= data->node_count)
  {
    return NULL;
  }

  return &data->nodes[node_index];
}
