/* Lossless RGBA8 / RGBA16 terrain heightmap round-trips. */
#define X_IMPL_TEST
#include <stdx/stdx_test.h>
#include <ldk_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static int s_png_roundtrip(LDKImageFormat format)
{
  const u32 w = 19u, h = 1200u; /* Multiple DEFLATE blocks in both formats. */
  const u32 bytes_per_channel = format == LDK_IMAGE_FORMAT_RGBA16_UNORM
      ? 2u : 1u;
  const size_t count = (size_t)w * h * 4u * bytes_per_channel;
  u8 *pixels = malloc(count);
  ASSERT_TRUE(pixels != NULL);
  for (size_t i = 0u; i < count; ++i)
    pixels[i] = (u8)((i * 73u + (i >> 3u) * 29u) & 255u);
  LDKImage *original = ldk_image_create_format(w, h, format, pixels);
  ASSERT_TRUE(original != NULL);
  const char *path = "ldk-image-png-roundtrip.tmp.png";
  ASSERT_TRUE(ldk_image_save_png(original, path));
  LDKImage *restored = ldk_image_load(path);
  ASSERT_TRUE(restored != NULL);
  LDKImageInfo read = {0};
  ASSERT_TRUE(ldk_image_get_info(restored, &read));
  ASSERT_EQ(read.width, w);
  ASSERT_EQ(read.height, h);
  ASSERT_EQ(read.format, format);
  ASSERT_EQ(read.byte_count, count);
  ASSERT_EQ(memcmp(read.pixels, pixels, count), 0);
  ldk_image_destroy(restored);
  ldk_image_destroy(original);
  free(pixels);
  remove(path);
  return 0;
}

/* Replacing a directory cannot succeed, but Save must not remove it.
 * This also verifies that temporary PNG files are cleaned up on failure. */
static int s_png_preserves_target_on_failure(void)
{
  static const char path[] = "ldk-image-png-existing-directory.tmp";
  const u8 rgba[4] = {10u, 20u, 30u, 255u};
  LDKImage *image = ldk_image_create(1u, 1u, rgba);
  ASSERT_TRUE(image != NULL);
#ifdef _WIN32
  ASSERT_EQ(_mkdir(path), 0);
#else
  ASSERT_EQ(mkdir(path, 0700), 0);
#endif
  ASSERT_FALSE(ldk_image_save_png(image, path));
#ifdef _WIN32
  ASSERT_EQ(_rmdir(path), 0);
#else
  ASSERT_EQ(rmdir(path), 0);
#endif
  ldk_image_destroy(image);
  return 0;
}

int main(void)
{
  int status = s_png_roundtrip(LDK_IMAGE_FORMAT_RGBA8_UNORM);
  if (status != 0) return status;
  status = s_png_roundtrip(LDK_IMAGE_FORMAT_RGBA16_UNORM);
  if (status != 0) return status;
  return s_png_preserves_target_on_failure();
}
