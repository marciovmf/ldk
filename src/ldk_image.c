#include <ldk_image.h>

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#ifndef LDK_IMAGE_MALLOC
#define LDK_IMAGE_MALLOC(size) malloc(size)
#endif

#ifndef LDK_IMAGE_FREE
#define LDK_IMAGE_FREE(ptr) free(ptr)
#endif

#define LDK_IMAGE_CHANNEL_COUNT 4u

struct LDKImage
{
  u32 width;
  u32 height;
  u32 channel_count;
  u64 byte_count;
  u8* pixels;
};

static bool ldk_image_compute_byte_count(u32 width, u32 height, u64* out_byte_count)
{
  if (!out_byte_count)
  {
    return false;
  }

  u64 pixel_count = (u64)width * (u64)height;
  u64 byte_count = pixel_count * (u64)LDK_IMAGE_CHANNEL_COUNT;

  if (width == 0 || height == 0)
  {
    return false;
  }

  if (pixel_count > (UINT64_MAX / (u64)LDK_IMAGE_CHANNEL_COUNT))
  {
    return false;
  }

  *out_byte_count = byte_count;
  return true;
}

LDKImage* ldk_image_create(u32 width, u32 height, void const* pixels)
{
  u64 byte_count = 0;

  if (!pixels)
  {
    return NULL;
  }

  if (!ldk_image_compute_byte_count(width, height, &byte_count))
  {
    return NULL;
  }

  LDKImage* image = (LDKImage*)LDK_IMAGE_MALLOC(sizeof(*image));

  if (!image)
  {
    return NULL;
  }

  memset(image, 0, sizeof(*image));

  image->pixels = (u8*)LDK_IMAGE_MALLOC((size_t)byte_count);

  if (!image->pixels)
  {
    LDK_IMAGE_FREE(image);
    return NULL;
  }

  memcpy(image->pixels, pixels, (size_t)byte_count);

  image->width = width;
  image->height = height;
  image->channel_count = LDK_IMAGE_CHANNEL_COUNT;
  image->byte_count = byte_count;

  return image;
}

LDKImage* ldk_image_load(char const* path)
{
  if (!path)
  {
    return NULL;
  }

  int width = 0;
  int height = 0;
  int source_channels = 0;

  stbi_uc* pixels = stbi_load(
      path,
      &width,
      &height,
      &source_channels,
      (int)LDK_IMAGE_CHANNEL_COUNT);

  if (!pixels)
  {
    return NULL;
  }

  if (width <= 0 || height <= 0)
  {
    stbi_image_free(pixels);
    return NULL;
  }

  LDKImage* image = ldk_image_create((u32)width, (u32)height, pixels);

  stbi_image_free(pixels);

  return image;
}

LDKImage* ldk_image_create_from_memory(void const* data, u32 data_size)
{
  if (!data || data_size == 0)
  {
    return NULL;
  }

  if (data_size > (u32)INT_MAX)
  {
    return NULL;
  }

  int width = 0;
  int height = 0;
  int source_channels = 0;

  stbi_uc* pixels = stbi_load_from_memory(
      (stbi_uc const*)data,
      (int)data_size,
      &width,
      &height,
      &source_channels,
      (int)LDK_IMAGE_CHANNEL_COUNT);

  if (!pixels)
  {
    return NULL;
  }

  if (width <= 0 || height <= 0)
  {
    stbi_image_free(pixels);
    return NULL;
  }

  LDKImage* image = ldk_image_create((u32)width, (u32)height, pixels);

  stbi_image_free(pixels);

  return image;
}

void ldk_image_destroy(LDKImage* image)
{
  if (!image)
  {
    return;
  }

  if (image->pixels)
  {
    LDK_IMAGE_FREE(image->pixels);
  }

  LDK_IMAGE_FREE(image);
}

bool ldk_image_get_info(LDKImage const* image, LDKImageInfo* out_info)
{
  if (!image || !out_info)
  {
    return false;
  }

  out_info->width = image->width;
  out_info->height = image->height;
  out_info->channel_count = image->channel_count;
  out_info->byte_count = image->byte_count;
  out_info->pixels = image->pixels;

  return true;
}

u32 ldk_image_get_width(LDKImage const* image)
{
  if (!image)
  {
    return 0;
  }

  return image->width;
}

u32 ldk_image_get_height(LDKImage const* image)
{
  if (!image)
  {
    return 0;
  }

  return image->height;
}

u32 ldk_image_get_channel_count(LDKImage const* image)
{
  if (!image)
  {
    return 0;
  }

  return image->channel_count;
}

u8 const* ldk_image_get_pixels(LDKImage const* image)
{
  if (!image)
  {
    return NULL;
  }

  return image->pixels;
}

u64 ldk_image_get_byte_count(LDKImage const* image)
{
  if (!image)
  {
    return 0;
  }

  return image->byte_count;
}
