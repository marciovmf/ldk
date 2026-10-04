#include <ldk_image.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

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
  u32 bytes_per_channel;
  LDKImageFormat format;
  u64 byte_count;
  u8* pixels;
};

static u32 s_image_format_bytes_per_channel(LDKImageFormat format)
{
  switch (format)
  {
  case LDK_IMAGE_FORMAT_RGBA8_UNORM: return 1u;
  case LDK_IMAGE_FORMAT_RGBA16_UNORM: return 2u;
  default: return 0u;
  }
}

static bool s_image_compute_byte_count(u32 width, u32 height,
    LDKImageFormat format, u64* out_byte_count)
{
  u32 bytes_per_channel;
  u64 pixel_count;
  u64 bytes_per_pixel;

  if (!out_byte_count || width == 0u || height == 0u)
  {
    return false;
  }

  bytes_per_channel = s_image_format_bytes_per_channel(format);
  if (bytes_per_channel == 0u)
  {
    return false;
  }

  pixel_count = (u64)width * (u64)height;
  bytes_per_pixel = (u64)LDK_IMAGE_CHANNEL_COUNT * bytes_per_channel;
  if (pixel_count > UINT64_MAX / bytes_per_pixel)
  {
    return false;
  }

  *out_byte_count = pixel_count * bytes_per_pixel;
  return true;
}

LDKImage* ldk_image_create_format(
    u32 width, u32 height, LDKImageFormat format, void const* pixels)
{
  u64 byte_count = 0;
  u32 bytes_per_channel;

  if (!pixels || !s_image_compute_byte_count(width, height, format, &byte_count) ||
      byte_count > SIZE_MAX)
  {
    return NULL;
  }

  bytes_per_channel = s_image_format_bytes_per_channel(format);
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
  image->bytes_per_channel = bytes_per_channel;
  image->format = format;
  image->byte_count = byte_count;
  return image;
}

LDKImage* ldk_image_create(u32 width, u32 height, void const* pixels)
{
  return ldk_image_create_format(
      width, height, LDK_IMAGE_FORMAT_RGBA8_UNORM, pixels);
}

static LDKImage* s_image_from_stbi_8(
    stbi_uc* pixels, int width, int height)
{
  if (!pixels || width <= 0 || height <= 0)
  {
    return NULL;
  }
  return ldk_image_create_format((u32)width, (u32)height,
      LDK_IMAGE_FORMAT_RGBA8_UNORM, pixels);
}

static LDKImage* s_image_from_stbi_16(
    stbi_us* pixels, int width, int height)
{
  if (!pixels || width <= 0 || height <= 0)
  {
    return NULL;
  }
  return ldk_image_create_format((u32)width, (u32)height,
      LDK_IMAGE_FORMAT_RGBA16_UNORM, pixels);
}

LDKImage* ldk_image_load(char const* path)
{
  int width = 0;
  int height = 0;
  int source_channels = 0;
  LDKImage* image = NULL;

  if (!path)
  {
    return NULL;
  }

  if (stbi_is_16_bit(path))
  {
    stbi_us* pixels = stbi_load_16(path, &width, &height, &source_channels,
        (int)LDK_IMAGE_CHANNEL_COUNT);
    image = s_image_from_stbi_16(pixels, width, height);
    stbi_image_free(pixels);
  }
  else
  {
    stbi_uc* pixels = stbi_load(path, &width, &height, &source_channels,
        (int)LDK_IMAGE_CHANNEL_COUNT);
    image = s_image_from_stbi_8(pixels, width, height);
    stbi_image_free(pixels);
  }

  return image;
}

LDKImage* ldk_image_create_from_memory(void const* data, u32 data_size)
{
  int width = 0;
  int height = 0;
  int source_channels = 0;
  LDKImage* image = NULL;

  if (!data || data_size == 0u || data_size > (u32)INT_MAX)
  {
    return NULL;
  }

  if (stbi_is_16_bit_from_memory((stbi_uc const*)data, (int)data_size))
  {
    stbi_us* pixels = stbi_load_16_from_memory((stbi_uc const*)data,
        (int)data_size, &width, &height, &source_channels,
        (int)LDK_IMAGE_CHANNEL_COUNT);
    image = s_image_from_stbi_16(pixels, width, height);
    stbi_image_free(pixels);
  }
  else
  {
    stbi_uc* pixels = stbi_load_from_memory((stbi_uc const*)data,
        (int)data_size, &width, &height, &source_channels,
        (int)LDK_IMAGE_CHANNEL_COUNT);
    image = s_image_from_stbi_8(pixels, width, height);
    stbi_image_free(pixels);
  }

  return image;
}

void ldk_image_destroy(LDKImage* image)
{
  if (!image)
  {
    return;
  }
  LDK_IMAGE_FREE(image->pixels);
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
  out_info->bytes_per_channel = image->bytes_per_channel;
  out_info->format = image->format;
  out_info->byte_count = image->byte_count;
  out_info->pixels = image->pixels;
  return true;
}

u32 ldk_image_get_width(LDKImage const* image)
{
  return image ? image->width : 0u;
}

u32 ldk_image_get_height(LDKImage const* image)
{
  return image ? image->height : 0u;
}

u32 ldk_image_get_channel_count(LDKImage const* image)
{
  return image ? image->channel_count : 0u;
}

u32 ldk_image_get_bytes_per_channel(LDKImage const* image)
{
  return image ? image->bytes_per_channel : 0u;
}

LDKImageFormat ldk_image_get_format(LDKImage const* image)
{
  return image ? image->format : LDK_IMAGE_FORMAT_RGBA8_UNORM;
}

u8 const* ldk_image_get_pixels(LDKImage const* image)
{
  return image ? image->pixels : NULL;
}

u64 ldk_image_get_byte_count(LDKImage const* image)
{
  return image ? image->byte_count : 0u;
}
