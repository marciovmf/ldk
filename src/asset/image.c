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

static uint32 magentaPixel = 0xFFFF00FF;

bool ldkAssetImageLoadFunc(const char* path, LDKAsset asset)
{
  size_t fileSize;
  unsigned char* fileMemory = ldkOsFileReadOffset(path, &fileSize, 0, 0);
  if (fileMemory == NULL)
  {
    LDKImage* image = (LDKImage*) asset;
    image->data = (unsigned char*) &magentaPixel;
    image->width = 1;
    image->height = 1;
    image->raw = true;
    return false;
  }

  LDKImage* image = (LDKImage*) asset;

  if (ldkGraphicsApiName() & LDK_GRAPHICS_OPENGL)
    stbi_set_flip_vertically_on_load(true);

  image->data = stbi_load_from_memory(fileMemory, (int) fileSize,
      (int32*) &image->width, (int32*) &image->height, NULL, 4);
  ldkOsMemoryFree(fileMemory);
  return true;
}

void ldkAssetImageUnloadFunc(LDKAsset asset)
{
  LDKImage* image = (LDKImage*) asset;

  if (image->data == (unsigned char*) &magentaPixel) 
    return;

  if (image->raw)
    ldkOsMemoryFree(image->data);
  else
    stbi_image_free(image->data);
}

