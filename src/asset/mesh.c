#include "ldk/asset/mesh.h"
#include "ldk/module/asset.h"
#include "ldk/module/renderer.h"
#include "ldk/os.h"
#include <string.h>
#include <stdlib.h>

#ifdef LDK_OS_WINDOWS
#define strtok_r strtok_s
#endif


//TODO(marcio): Move this function to common
static char* internalSkipWhiteSpace(char* input)
{
  if (input == NULL)
    return input;

  char* p = input;
  while(*p == ' ' || *p == '\t' || *p == '\r')
    p++;
  return p;
}

static bool internalParseInt(const char* path, int line, const char* input, int32* out)
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

static bool internalParseUInt(const char* path, int line, const char* input, uint32* out)
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

static bool internalParseFloat(const char* path, int line, const char* input, float* out)
{
  int dotCount = 0;
  int len = 0;
  bool error = false;

  if ((*input == '-' || *input == '+') && *(input + 1) != 0)
  {
    len++;
    input++;
  }

  while(*input)
  {
    len++;
    if (*input == '.') { dotCount++; }

    if (dotCount > 1 || (*input != '.' && (*input < '0' || *input > '9' )))
    {
      error = true;
      break;
    }
    input++;
  }

  // Floats can not end with a dot or contain just a dot
  input -=len;
  error |= (len == 0 || input[len-1] == '.');
  error |= (len == 1 && input[0] == '.');

  if (error)
  {
    ldkLogError("%s at line %d: Error parsing float value", path, line);
    return false;
  }

  *out = (float) atof(input);
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
  LDKHandle* materials = NULL;
  size_t vertexBufferSize = 0;
  size_t indexBufferSize = 0;

  while (line)
  {
    lineNumber++;
    line = internalSkipWhiteSpace(line);
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

      lhs = internalSkipWhiteSpace(lhs);
      rhs = internalSkipWhiteSpace(rhs);

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
        internalParseUInt(path, lineNumber, rhs, &mesh->numVertices);
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
        internalParseUInt(path, lineNumber, rhs, &mesh->numIndices);
        indexBufferSize = mesh->numIndices * (sizeof(uint16));
        mesh->indices = (uint16*) ldkOsMemoryAlloc(indexBufferSize);
        indices = mesh->indices;
      }
      else if (strncmp("material_count", lhs, MAX_LHS_SIZE) == 0)
      {
        internalParseUInt(path, lineNumber, rhs, &mesh->numMaterials);
        mesh->materials = (LDKHandle*) ldkOsMemoryAlloc(mesh->numMaterials * (sizeof(LDKHandle)));
        materials = mesh->materials;
      }
      else if (strncmp("surface_count", lhs, MAX_LHS_SIZE) == 0)
      {
        internalParseUInt(path, lineNumber, rhs, &mesh->numSurfaces);
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
        internalParseFloat(path, lineNumber, x, &pos.x);
        internalParseFloat(path, lineNumber, y, &pos.y);
        internalParseFloat(path, lineNumber, z, &pos.z);

        Vec3 normal;
        x = strtok_r(NULL, SPACE_OR_TAB, &context);
        y = strtok_r(NULL, SPACE_OR_TAB, &context);
        z = strtok_r(NULL, SPACE_OR_TAB, &context);
        internalParseFloat(path, lineNumber, x, &normal.x);
        internalParseFloat(path, lineNumber, y, &normal.y);
        internalParseFloat(path, lineNumber, z, &normal.z);

        Vec2 uv;
        x = strtok_r(NULL,  SPACE_OR_TAB, &context);
        y = strtok_r(NULL, SPACE_OR_TAB, &context);
        internalParseFloat(path, lineNumber, x, &uv.x);
        internalParseFloat(path, lineNumber, y, &uv.y);

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
          internalParseUInt(path, lineNumber, strIndex, &index);
          *indices++ = (uint16) index;
          strIndex = strtok_r(NULL,  SPACE_OR_TAB, &context);
        }
      }
      else if (strncmp("material", lhs, MAX_LHS_SIZE) == 0)
      {
        char* matId   = rhs;
        char* matName = strtok_r(NULL, SPACE_OR_TAB, &context);

        uint32 materialId;
        internalParseUInt(path, lineNumber, matId, &materialId);
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

        internalParseInt(path, lineNumber, materialId, &valueMaterialId);
        internalParseInt(path, lineNumber, indexStart, &valueIndexStart);
        internalParseInt(path, lineNumber, indexCount, &valueIndexCount);

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
        internalParseFloat(path, lineNumber, x, &mesh->boundingSphere.center.x);
        internalParseFloat(path, lineNumber, y, &mesh->boundingSphere.center.y);
        internalParseFloat(path, lineNumber, z, &mesh->boundingSphere.center.z);
        internalParseFloat(path, lineNumber, r, &mesh->boundingSphere.radius);
      }
      else if (strncmp("bounding_box", lhs, strlen(lhs)) == 0)
      {
        char* minX = rhs;
        char* minY = strtok_r(NULL, SPACE_OR_TAB, &context);
        char* maxX = strtok_r(NULL, SPACE_OR_TAB, &context);
        char* maxY = strtok_r(NULL, SPACE_OR_TAB, &context);
        internalParseFloat(path, lineNumber, minX, &mesh->boundingBox.minX);
        internalParseFloat(path, lineNumber, minY, &mesh->boundingBox.minY);
        internalParseFloat(path, lineNumber, maxX, &mesh->boundingBox.maxX);
        internalParseFloat(path, lineNumber, maxY, &mesh->boundingBox.maxY);
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


  const int32 stride = (int32) (8 * sizeof(float));
  mesh->vBuffer = ldkRenderBufferCreate(1);
  ldkVertexBufferSetAttributeFloat(mesh->vBuffer, 0, LDK_VERTEX_ATTRIBUTE_POSITION, 3, stride, 0, 0);
  ldkVertexBufferSetAttributeFloat(mesh->vBuffer, 0, LDK_VERTEX_ATTRIBUTE_NORMAL, 3, stride, (const void*) (3 * sizeof(float)), 0);
  ldkVertexBufferSetAttributeFloat(mesh->vBuffer, 0, LDK_VERTEX_ATTRIBUTE_TEXCOORD0, 2, stride, (const void* )(6 * sizeof(float)), 0);
  ldkVertexBufferSetData(mesh->vBuffer, 0, vertexBufferSize, mesh->vertices, false);
  ldkIndexBufferSetData(mesh->vBuffer, indexBufferSize, mesh->indices, false);
  ldkRenderBufferBind(NULL);

  return true;
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
