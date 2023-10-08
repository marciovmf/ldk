#include "ldk/asset/image.h"
#include "ldk/module/graphics.h"
#include "ldk/os.h"
#include "ldk/os.h"

#define STB_IMAGE_IMPLEMENTATION

#ifdef LDK_DEBUG
#define STBI_ONLY_PSD
#endif
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STBI_ONLY_HDR

#define STBI_MALLOC  ldkOsMemoryAlloc
#define STBI_FREE    ldkOsMemoryFree
#define STBI_REALLOC ldkOsMemoryResize
#include "../depend/stb_image.h"


LDKHImage ldkAssetImageLoadFunc(const char* path)
{
  size_t fileSize;
  unsigned char* fileMemory = ldkOsFileReadOffset(path, &fileSize, 0, 0);

  if (fileMemory == NULL)
  {
    ldkLogError("Unable to load image '%s'", path);
    return LDK_HANDLE_INVALID;
  }

  LDKImage* image = (LDKImage*) ldkOsMemoryAlloc(sizeof(LDKImage));

  if (ldkGraphicsApiName() & LDK_GRAPHICS_OPENGL)
    stbi_set_flip_vertically_on_load(true);

  image->data = stbi_load_from_memory(fileMemory, (int) fileSize,
      (int32*) &image->width, (int32*) &image->height, NULL, 4);
  ldkOsMemoryFree(fileMemory);
  return (LDKHImage) image;
}

void ldkAssetImageUnloadFunc(LDKHImage image)
{
  ldkOsMemoryFree((LDKImage*) image);
}

LDKImage* ldkAssetImageGetPointer(LDKHImage handle)
{
  return (LDKImage*) handle;
}

