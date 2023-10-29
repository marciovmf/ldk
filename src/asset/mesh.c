#include "ldk/module/asset.h"
#include "ldk/asset/mesh.h"
#include "ldk/os.h"
#include "ldk/module/renderer.h"
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

LDKHMesh ldkAssetMeshLoadFunc(const char* path)
{
  size_t fileSize = 0;
  byte* buffer = ldkOsFileReadOffset(path, &fileSize, 1, 0);
  if (buffer == NULL)
    return LDK_HANDLE_INVALID;

  buffer[fileSize] = 0;

  const int32 MAX_LHS_SIZE = 64;
  const char* LINEBREAK = "\n\r";
  const char* SPACE_OR_TAB = "  \t";
  char* lineBreakContext;
  int lineNumber = 0;
  LDKVertexLayout vertexLayout = LDK_VERTEX_LAYOUT_NONE;
  char* line = strtok_r((char*) buffer, LINEBREAK, &lineBreakContext);

  LDKMesh* mesh   = (LDKMesh*)  ldkOsMemoryAlloc(sizeof(LDKMesh));
  memset((void*) mesh, 0, sizeof(LDKMesh));
  bool error = false;

  float* vertices = NULL;
  uint16* indices = NULL;
  LDKSurface* surfaces = NULL;
  LDKHMaterial* materials = NULL;
  size_t vertexBufferSize = 0;
  size_t indexBufferSize = 0;

  while (line)
  {
    lineNumber++;
    line = internalSkipWhiteSpace(line);
    if (line[0] != '#')
    {
      char* entryContext;
      char* lhs = strtok_r(line, ":", &entryContext);
      char* rhs = strtok_r(NULL, ":", &entryContext);

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
        mesh->materials = (LDKHMaterial*) ldkOsMemoryAlloc(mesh->numMaterials * (sizeof(LDKHMaterial)));
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

        char* context;
        Vec3 pos;
        char* x = strtok_r(rhs,  SPACE_OR_TAB, &context);
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
        char* context;
        char* strIndex = strtok_r(rhs, SPACE_OR_TAB, &context);
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
        char* context;
        char* matId   = strtok_r(rhs,  SPACE_OR_TAB, &context);
        char* matName = strtok_r(NULL, SPACE_OR_TAB, &context);

        uint32 materialId;
        internalParseUInt(path, lineNumber, matId, &materialId);
        LDK_ASSERT(materialId < mesh->numMaterials);
        *materials++ = ldkAssetGet(matName);
      }
      else if (strncmp("surface", lhs, MAX_LHS_SIZE) == 0)
      {
        char* context;
        char* materialId = strtok_r(rhs,  SPACE_OR_TAB, &context);
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
        char* context;
        char* x = strtok_r(rhs,  SPACE_OR_TAB, &context);
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
        char* context;
        char* minX = strtok_r(rhs,  SPACE_OR_TAB, &context);
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

  mesh->vBuffer = ldkVertexBufferCreate(LDK_VERTEX_LAYOUT_PNU);
  ldkVertexBufferData(mesh->vBuffer, (void*) mesh->vertices, vertexBufferSize);
  ldkVertexBufferIndexData(mesh->vBuffer, (void*) mesh->indices, indexBufferSize);
  return (LDKHMesh) mesh;
}

LDKVertexBuffer ldkAssetMeshGetVertexBuffer(LDKHMesh handle)
{
  LDKMesh* mesh = (LDKMesh*) handle;
  return mesh->vBuffer;
}

void ldkAssetMeshUnloadFunc(LDKHMesh handle)
{
  LDKMesh* mesh = (LDKMesh*) handle;

  ldkOsMemoryFree(mesh->vertices);
  ldkOsMemoryFree(mesh->materials);
  ldkOsMemoryFree(mesh->indices);
  ldkOsMemoryFree(mesh->surfaces);
  ldkVertexBufferDestroy(mesh->vBuffer);
  ldkOsMemoryFree(mesh);
}

LDKMesh* ldkAssetMeshGetPointer(LDKHMesh handle)
{
  return (LDKMesh*) handle;
}
