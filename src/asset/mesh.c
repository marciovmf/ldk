#include "ldk/asset/mesh.h"
#include "ldk/module/asset.h"
#include "ldk/os.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef LDK_OS_WINDOWS
#define strtok_r strtok_s
#endif


//TODO(marcio): Move this function to common
static char* s_skipWhiteSpace(char* input)
{
  if (input == NULL)
    return input;

  char* p = input;
  while(*p == ' ' || *p == '\t' || *p == '\r')
    p++;
  return p;
}

static bool s_parseInt(const char* path, int line, const char* input, int32* out)
{
  const char* p = input;
  if ((*input == '-' || *input == '+') && *(input + 1) != 0)
    input++;

  while(*input)
  {
    if (*input < '0' || *input > '9')
    {
      ldkLogError("%s at line %d: Error parsing integer value", path, line);
      return false;
    }
    input++;
  }

  *out = atoi(p);
  return true;
}

static bool s_parseUInt(const char* path, int line, const char* input, uint32* out)
{
  const char* p = input;
  if ((*input == '-' || *input == '+') && *(input + 1) != 0)
    input++;

  while(*input)
  {
    if (*input < '0' || *input > '9')
    {
      ldkLogError("%s at line %d: Error parsing integer value", path, line);
      return false;
    }
    input++;
  }

  *out = (uint32) strtoul(p, NULL, 10);
  return true;
}

static bool s_parseFloat(const char* path, int line, const char* input, float* out)
{
  int dotCount = 0;
  int eCount = 0; // expoent
  int len = 0;
  bool error = false;

  if ((*input == '-' || *input == '+') && *(input + 1) != 0)
  {
    len++;
    input++;
  }

  const char* expactSignAt = NULL;
  while(*input)
  {
    len++;
    if (*input == '.') { dotCount++; }
    else if (*input == 'e') { eCount++; expactSignAt = input+1;}

    if (dotCount > 1      // more than one dot
        || eCount > 1     // more than one 'e'
        || ((*input == '-' || *input == '+') && expactSignAt != input) // sign out of place ?
        || (*input != '-' && *input != '+' && *input != 'e' && *input != '.' && (*input < '0' || *input > '9')) ) // unexpected character ?
    {
      error = true;
      break;
    }
    input++;
  }

  // Floats can not end with or be only ". - + e"
  input -=len;
  error |= (len == 0 || input[len-1] == '.' || input[len-1] == '-' || input[len-1] == '+' || input[len-1] == 'e');
  error |= (len == 1 && (input[0] == '.' || input[0] == '-' || input[0] == '+' || input[0] == 'e'));

  if (error)
  {
    LDK_ASSERT_BREAK();
    ldkLogError("%s at line %d: Error parsing float value", path, line);
    return false;
  }

  char* endptr;
  *out = strtof(input, &endptr);
  LDK_ASSERT(*endptr == '\0' && errno != ERANGE);
  return true;
}

bool ldkAssetMeshLoadFunc(const char* path, LDKAsset asset)
{
  size_t fileSize = 0;
  LDKMesh* mesh = (LDKMesh*) asset;

  byte* buffer = ldkOsFileReadOffset(path, &fileSize, 1, 0);
  if (buffer == NULL)
  {
    mesh->vBuffer = NULL;
    mesh->numIndices = 0;
    mesh->numSurfaces = 0;
    mesh->numVertices = 0;
    mesh->numMaterials = 0;
    return false;
  }

  buffer[fileSize] = 0;

  const int32 MAX_LHS_SIZE = 64;
  const char* LINEBREAK = "\n\r";
  const char* SPACE_OR_TAB = "  \t";
  char* lineBreakContext;
  int lineNumber = 0;
  LDKVertexLayout vertexLayout = LDK_VERTEX_LAYOUT_NONE;
  char* line = strtok_r((char*) buffer, LINEBREAK, &lineBreakContext);

  mesh->vertices = 0;
  mesh->indices = 0;
  mesh->materials = 0;
  mesh->surfaces = 0;
  mesh->numVertices = 0;
  mesh->numIndices = 0;
  mesh->numSurfaces = 0;
  mesh->numMaterials = 0;
  memset(&mesh->boundingBox, 0, sizeof(LDKBoundingBox));
  memset(&mesh->boundingSphere, 0, sizeof(LDKBoundingSphere));

  bool error = false;

  float* vertices = NULL;
  uint16* indices = NULL;
  LDKSurface* surfaces = NULL;
  LDKHAsset* materials = NULL;
  size_t vertexBufferSize = 0;
  size_t indexBufferSize = 0;

  while (line)
  {
    lineNumber++;
    line = s_skipWhiteSpace(line);
    if (line[0] != '#')
    {
      char* context;
      char* lhs = strtok_r(line, SPACE_OR_TAB, &context);
      char* rhs = strtok_r(NULL, SPACE_OR_TAB, &context);

      if (lhs == NULL || rhs == NULL)
      {
        ldkLogError("Error parsing mesh file '%s' at line %d: Invalid entry format.", path, lineNumber);
        error = true;
        break;
      }

      lhs = s_skipWhiteSpace(lhs);
      rhs = s_skipWhiteSpace(rhs);

      if (strncmp("version", lhs, strlen(lhs)) == 0)
      {
        if (strncmp(rhs,"1.0", strlen(rhs)) != 0)
        {
          ldkLogError("Incompatible version. Supoprted version is 1.0");
          error = true;
          break;
        }
      }
      else if (strncmp("vertex_format", lhs, MAX_LHS_SIZE) == 0)
      {
        // PNUV is the only format supported for
        if (strncmp(rhs,"PNU", strlen(rhs)) == 0)
        {
          vertexLayout = LDK_VERTEX_LAYOUT_PNU;
        }
        else
        {
          ldkLogError("Unsuported vertex format '%s'.", rhs);
          error = true;
          break;
        }
      }
      else if (strncmp("vertex_count", lhs, MAX_LHS_SIZE) == 0)
      {
        LDK_ASSERT(vertexLayout != LDK_VERTEX_LAYOUT_NONE);
        s_parseUInt(path, lineNumber, rhs, &mesh->numVertices);
        if (vertexLayout == LDK_VERTEX_LAYOUT_PNU)
        {
          vertexBufferSize = mesh->numVertices * (sizeof(Vec3) + sizeof(Vec3) + sizeof(Vec2));
          mesh->vertices = (float*) ldkOsMemoryAlloc(vertexBufferSize);
          vertices = mesh->vertices;
        }
        else
        {
          LDK_NOT_IMPLEMENTED();
          error = true;
        };
      }
      else if (strncmp("index_count", lhs, MAX_LHS_SIZE) == 0)
      {
        s_parseUInt(path, lineNumber, rhs, &mesh->numIndices);
        indexBufferSize = mesh->numIndices * (sizeof(uint16));
        mesh->indices = (uint16*) ldkOsMemoryAlloc(indexBufferSize);
        indices = mesh->indices;
      }
      else if (strncmp("material_count", lhs, MAX_LHS_SIZE) == 0)
      {
        s_parseUInt(path, lineNumber, rhs, &mesh->numMaterials);
        mesh->materials = (LDKHAsset*) ldkOsMemoryAlloc(mesh->numMaterials * (sizeof(LDKHAsset)));
        materials = mesh->materials;
      }
      else if (strncmp("surface_count", lhs, MAX_LHS_SIZE) == 0)
      {
        s_parseUInt(path, lineNumber, rhs, &mesh->numSurfaces);
        mesh->surfaces = (LDKSurface*) ldkOsMemoryAlloc(mesh->numSurfaces * (sizeof(LDKSurface)));
        surfaces = mesh->surfaces;
      }
      else if (strncmp("vertex", lhs, MAX_LHS_SIZE) == 0)
      {
        LDK_ASSERT(mesh != NULL && mesh->vertices != NULL);

        Vec3 pos;
        char* x = rhs;//strtok_r(NULL,  SPACE_OR_TAB, &context);
        char* y = strtok_r(NULL, SPACE_OR_TAB, &context);
        char* z = strtok_r(NULL, SPACE_OR_TAB, &context);
        s_parseFloat(path, lineNumber, x, &pos.x);
        s_parseFloat(path, lineNumber, y, &pos.y);
        s_parseFloat(path, lineNumber, z, &pos.z);

        Vec3 normal;
        x = strtok_r(NULL, SPACE_OR_TAB, &context);
        y = strtok_r(NULL, SPACE_OR_TAB, &context);
        z = strtok_r(NULL, SPACE_OR_TAB, &context);
        s_parseFloat(path, lineNumber, x, &normal.x);
        s_parseFloat(path, lineNumber, y, &normal.y);
        s_parseFloat(path, lineNumber, z, &normal.z);

        Vec2 uv;
        x = strtok_r(NULL,  SPACE_OR_TAB, &context);
        y = strtok_r(NULL, SPACE_OR_TAB, &context);
        s_parseFloat(path, lineNumber, x, &uv.x);
        s_parseFloat(path, lineNumber, y, &uv.y);

        *vertices++ = pos.x;
        *vertices++ = pos.y;
        *vertices++ = pos.z;
        *vertices++ = normal.x;
        *vertices++ = normal.y;
        *vertices++ = normal.z;
        *vertices++ = uv.x;
        *vertices++ = uv.y;
      }
      else if (strncmp("index_list", lhs, MAX_LHS_SIZE) == 0)
      {
        char* strIndex = rhs;
        while (strIndex)
        {
          uint32 index;
          s_parseUInt(path, lineNumber, strIndex, &index);
          *indices++ = (uint16) index;
          strIndex = strtok_r(NULL,  SPACE_OR_TAB, &context);
        }
      }
      else if (strncmp("material", lhs, MAX_LHS_SIZE) == 0)
      {
        char* matId   = rhs;
        char* matName = strtok_r(NULL, SPACE_OR_TAB, &context);

        uint32 materialId;
        s_parseUInt(path, lineNumber, matId, &materialId);
        LDK_ASSERT(materialId < mesh->numMaterials);
        LDKMaterial* material = ldkAssetGet(LDKMaterial, matName);
        *materials++ = material->asset.handle;
      }
      else if (strncmp("surface", lhs, MAX_LHS_SIZE) == 0)
      {
        char* materialId = rhs;
        char* indexStart = strtok_r(NULL, SPACE_OR_TAB, &context);
        char* indexCount = strtok_r(NULL, SPACE_OR_TAB, &context);

        int32 valueMaterialId;
        int32 valueIndexStart;
        int32 valueIndexCount;

        s_parseInt(path, lineNumber, materialId, &valueMaterialId);
        s_parseInt(path, lineNumber, indexStart, &valueIndexStart);
        s_parseInt(path, lineNumber, indexCount, &valueIndexCount);

        surfaces->first = valueIndexStart;
        surfaces->count = valueIndexCount;
        surfaces->materialIndex = valueMaterialId;
        surfaces++;
      }
      else if (strncmp("bounding_sphere", lhs, MAX_LHS_SIZE) == 0)
      {
        char* x = rhs;
        char* y = strtok_r(NULL, SPACE_OR_TAB, &context);
        char* z = strtok_r(NULL, SPACE_OR_TAB, &context);
        char* r = strtok_r(NULL, SPACE_OR_TAB, &context);
        s_parseFloat(path, lineNumber, x, &mesh->boundingSphere.center.x);
        s_parseFloat(path, lineNumber, y, &mesh->boundingSphere.center.y);
        s_parseFloat(path, lineNumber, z, &mesh->boundingSphere.center.z);
        s_parseFloat(path, lineNumber, r, &mesh->boundingSphere.radius);
      }
      else if (strncmp("bounding_box", lhs, strlen(lhs)) == 0)
      {
        char* minX = rhs;
        char* minY = strtok_r(NULL, SPACE_OR_TAB, &context);
        char* maxX = strtok_r(NULL, SPACE_OR_TAB, &context);
        char* maxY = strtok_r(NULL, SPACE_OR_TAB, &context);
        s_parseFloat(path, lineNumber, minX, &mesh->boundingBox.minX);
        s_parseFloat(path, lineNumber, minY, &mesh->boundingBox.minY);
        s_parseFloat(path, lineNumber, maxX, &mesh->boundingBox.maxX);
        s_parseFloat(path, lineNumber, maxY, &mesh->boundingBox.maxY);
      }
    }
    line = strtok_r(NULL, LINEBREAK, &lineBreakContext);
  }

  if (error)
  {
    ldkOsMemoryFree(mesh->vertices);
    ldkOsMemoryFree(mesh->indices);
    ldkOsMemoryFree(mesh->surfaces);
    ldkOsMemoryFree(mesh->materials);
    ldkOsMemoryFree(mesh);
  }

  mesh->vBuffer = ldkRenderBufferCreate(1);
  const int32 stride = (int32) (8 * sizeof(float));
  ldkRenderBufferBind(mesh->vBuffer);

  ldkRenderBufferAttributeSetFloat(mesh->vBuffer, 0, LDK_VERTEX_ATTRIBUTE_POSITION, 3, stride, 0, 0);
  ldkRenderBufferAttributeSetFloat(mesh->vBuffer, 0, LDK_VERTEX_ATTRIBUTE_NORMAL, 3, stride, (const void*) (3 * sizeof(float)), 0);
  ldkRenderBufferAttributeSetFloat(mesh->vBuffer, 0, LDK_VERTEX_ATTRIBUTE_TEXCOORD0, 2, stride, (const void* )(6 * sizeof(float)), 0);
  ldkRenderBufferSetVertexData(mesh->vBuffer, 0, vertexBufferSize, mesh->vertices, false);

  ldkRenderBufferSetIndexData(mesh->vBuffer, indexBufferSize, mesh->indices, false);
  ldkRenderBufferBind(NULL);

  return true;
}

LDKMesh* ldkQuadMeshCreate(LDKHAsset material)
{
  LDKMesh* mesh = ldkAssetNew(LDKMesh);
  memset(&mesh->boundingBox, 0, sizeof(LDKBoundingBox));
  memset(&mesh->boundingSphere, 0, sizeof(LDKBoundingSphere));

  // Indices
  uint16 indices[] = { 0, 1, 2, 1, 3, 2 };
  mesh->numIndices = 6;
  mesh->indices = (uint16*) ldkOsMemoryAlloc(sizeof(indices));
  memcpy(mesh->indices, indices, sizeof(indices));

  // Vertices
  float vertices[] = {
    // Position          // Normal           // UV
    -1.0f,  1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,
    1.0f,  1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f,
    1.0f, -1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f
  };
  mesh->numVertices = 4;
  mesh->vertices = (float*) ldkOsMemoryAlloc(sizeof(vertices));
  memcpy(mesh->vertices, vertices, sizeof(vertices));

  // surfaces
  mesh->numSurfaces = 1;
  mesh->surfaces = (LDKSurface*) ldkOsMemoryAlloc(sizeof(LDKSurface));
  mesh->surfaces->count = 6;
  mesh->surfaces->first = 0;
  mesh->surfaces->materialIndex = 0;

  // materials
  mesh->numMaterials = 1;
  mesh->materials = (LDKHAsset*) ldkOsMemoryAlloc(sizeof(LDKHAsset));
  *mesh->materials = material;

  // RenderBuffer
  mesh->vBuffer = ldkRenderBufferCreate(1);
  const int32 stride = (int32) (8 * sizeof(float));
  ldkRenderBufferBind(mesh->vBuffer);

  ldkRenderBufferAttributeSetFloat(mesh->vBuffer, 0, LDK_VERTEX_ATTRIBUTE_POSITION, 3, stride, 0, 0);
  ldkRenderBufferAttributeSetFloat(mesh->vBuffer, 0, LDK_VERTEX_ATTRIBUTE_NORMAL, 3, stride, (const void*) (3 * sizeof(float)), 0);
  ldkRenderBufferAttributeSetFloat(mesh->vBuffer, 0, LDK_VERTEX_ATTRIBUTE_TEXCOORD0, 2, stride, (const void*)(6 * sizeof(float)), 0);
  ldkRenderBufferSetVertexData(mesh->vBuffer, 0, sizeof(vertices), mesh->vertices, false);

  ldkRenderBufferSetIndexData(mesh->vBuffer, sizeof(indices), mesh->indices, false);
  ldkRenderBufferBind(NULL);

  return mesh;
}

void ldkAssetMeshUnloadFunc(LDKAsset asset)
{
  LDKMesh* mesh = (LDKMesh*) asset;
  ldkOsMemoryFree(mesh->vertices);
  ldkOsMemoryFree(mesh->materials);
  ldkOsMemoryFree(mesh->indices);
  ldkOsMemoryFree(mesh->surfaces);
  ldkRenderBufferDestroy(mesh->vBuffer);
  ldkOsMemoryFree(mesh);
}
