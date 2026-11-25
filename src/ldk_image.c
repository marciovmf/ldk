#include <ldk_image.h>

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

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

/* Portable PNG writer for terrain heightmaps. PNG encodes 16-bit channels
 * big-endian and zlib stores uncompressed DEFLATE blocks here, avoiding a
 * compression dependency while retaining every height sample exactly. */
static void s_png_u32be(u8 out[4], u32 value)
{
  out[0] = (u8)(value >> 24u);
  out[1] = (u8)(value >> 16u);
  out[2] = (u8)(value >> 8u);
  out[3] = (u8)value;
}

static bool s_png_chunk(FILE *file, const char type[4],
    const u8 *bytes, u32 size)
{
  const u8 len[4] = {
      (u8)(size >> 24u), (u8)(size >> 16u), (u8)(size >> 8u), (u8)size};
  u32 crc = 0xffffffffu;
  if (fwrite(len, 1u, 4u, file) != 4u ||
      fwrite(type, 1u, 4u, file) != 4u ||
      (size && fwrite(bytes, 1u, size, file) != size))
    return false;
  const u8 *type_bytes = (const u8 *)type;
  for (u64 i = 0u; i < (u64)size + 4u; ++i)
  {
    u8 value = i < 4u ? type_bytes[i] : bytes[i - 4u];
    crc ^= value;
    for (u32 bit = 0u; bit < 8u; ++bit)
      crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  u8 checksum[4];
  s_png_u32be(checksum, crc ^ 0xffffffffu);
  return fwrite(checksum, 1u, 4u, file) == 4u;
}

bool ldk_image_save_png(const LDKImage *image, const char *filename)
{
  LDKImageInfo info = {0};
  FILE *file = NULL;
  u8 *raw = NULL;
  u8 *encoded = NULL;
  char *temp_path = NULL;
  bool ok = false;
  if (!filename || !*filename ||
      !ldk_image_get_info(image, &info) || !info.width || !info.height ||
      info.channel_count != 4u ||
      (info.format != LDK_IMAGE_FORMAT_RGBA8_UNORM &&
       info.format != LDK_IMAGE_FORMAT_RGBA16_UNORM))
    return false;
  /* A scanline starts with one filter byte. Bound intermediate allocations
   * as well as the PNG IDAT length (which is a 32-bit unsigned field). */
  const u64 row_bytes = (u64)info.width * 4u * info.bytes_per_channel;
  if (row_bytes >= UINT64_MAX / info.height) return false;
  const u64 raw_size = (row_bytes + 1u) * info.height;
  const u64 blocks = raw_size / 65535u + (raw_size % 65535u != 0u);
  if (blocks > (UINT64_MAX - raw_size - 6u) / 5u) return false;
  const u64 encoded_size = 2u + raw_size + blocks * 5u + 4u;
  if (!row_bytes || raw_size > SIZE_MAX ||
      encoded_size > UINT32_MAX || encoded_size > SIZE_MAX)
    return false;
  raw = malloc((size_t)raw_size);
  encoded = malloc((size_t)encoded_size);
  if (!raw || !encoded) goto done;
  for (u32 y = 0u; y < info.height; ++y)
  {
    u8 *dst = raw + (size_t)y * (size_t)(row_bytes + 1u);
    const u8 *src = info.pixels + (size_t)y * (size_t)row_bytes;
    dst[0] = 0u; /* PNG filter: None */
    if (info.bytes_per_channel == 1u)
      memcpy(dst + 1u, src, (size_t)row_bytes);
    else
    {
      /* The image pixels are host-endian u16 values. PNG wants network order. */
      for (size_t i = 0u; i < (size_t)row_bytes / 2u; ++i)
      {
        u16 value;
        memcpy(&value, src + i * 2u, sizeof(value));
        dst[i * 2u + 1u] = (u8)(value >> 8u);
        dst[i * 2u + 2u] = (u8)value;
      }
    }
  }
  size_t out = 0u;
  encoded[out++] = 0x78u; /* zlib CMF: deflate, 32 KB window */
  encoded[out++] = 0x01u; /* FLG: fastest/no compression */
  u32 a = 1u, b = 0u;
  for (size_t i = 0u; i < (size_t)raw_size;)
  {
    size_t remaining = (size_t)raw_size - i;
    u16 n = (u16)(remaining < 65535u ? remaining : 65535u);
    encoded[out++] = remaining <= 65535u ? 0x01u : 0x00u;
    encoded[out++] = (u8)n;
    encoded[out++] = (u8)(n >> 8u);
    encoded[out++] = (u8)~n;
    encoded[out++] = (u8)(~n >> 8u);
    memcpy(encoded + out, raw + i, n);
    out += n;
    for (u32 k = 0u; k < n; ++k)
    {
      a += raw[i + k];
      b += a;
      a %= 65521u;
      b %= 65521u;
    }
    i += n;
  }
  s_png_u32be(encoded + out, (b << 16u) | a);
  out += 4u;
  if (out != (size_t)encoded_size) goto done;
  /* Complete the image in a sibling file before replacing a shared source.
   * A failed Save must never truncate or delete the previous heightmap. */
  size_t name_len = strlen(filename);
  static const char suffix[] = ".ldk.tmp";
  if (name_len > SIZE_MAX - sizeof(suffix)) goto done;
  temp_path = malloc(name_len + sizeof(suffix));
  if (!temp_path) goto done;
  memcpy(temp_path, filename, name_len);
  memcpy(temp_path + name_len, suffix, sizeof(suffix));
  file = fopen(temp_path, "wb");
  if (!file) goto done;
  static const u8 signature[8] = {137u,80u,78u,71u,13u,10u,26u,10u};
  u8 ihdr[13] = {0};
  s_png_u32be(ihdr, info.width);
  s_png_u32be(ihdr + 4u, info.height);
  ihdr[8] = info.bytes_per_channel == 1u ? 8u : 16u;
  ihdr[9] = 6u; /* RGBA */
  ok = fwrite(signature, 1u, sizeof(signature), file) == sizeof(signature) &&
      s_png_chunk(file, "IHDR", ihdr, sizeof(ihdr)) &&
      s_png_chunk(file, "IDAT", encoded, (u32)encoded_size) &&
      s_png_chunk(file, "IEND", NULL, 0u) &&
      fflush(file) == 0;
 done:
  if (file && fclose(file) != 0) ok = false;
  if (ok)
  {
#ifdef _WIN32
    ok = MoveFileExA(temp_path, filename,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = rename(temp_path, filename) == 0;
#endif
  }
  if (!ok && temp_path) (void)remove(temp_path);
  free(temp_path);
  free(raw);
  free(encoded);
  return ok;
}

bool ldk_image_pixels_set(LDKImage *image,
    const void *pixels, u64 byte_count)
{
  if (!image || !pixels || byte_count != image->byte_count)
    return false;
  memcpy(image->pixels, pixels, (size_t)byte_count);
  return true;
}

bool ldk_image_pixels_region_set(LDKImage *image,
    const LDKImageInfo *source, u32 x, u32 y, u32 width, u32 height)
{
  if (!image || !source || !source->pixels || !width || !height ||
      source->width != image->width ||
      source->height != image->height ||
      source->format != image->format ||
      source->channel_count != image->channel_count ||
      source->bytes_per_channel != image->bytes_per_channel ||
      source->byte_count != image->byte_count ||
      x >= image->width || y >= image->height ||
      width > image->width - x || height > image->height - y)
    return false;

  const size_t pixel_size = (size_t)image->channel_count *
      image->bytes_per_channel;
  const size_t stride = (size_t)image->width * pixel_size;
  const size_t row_bytes = (size_t)width * pixel_size;
  const size_t offset = (size_t)x * pixel_size;
  for (u32 row = 0u; row < height; ++row)
  {
    const size_t row_start = ((size_t)y + row) * stride + offset;
    memmove(image->pixels + row_start,
        source->pixels + row_start, row_bytes);
  }
  return true;
}
