#ifndef LDK_IMAGE_H
#define LDK_IMAGE_H

#include <ldk_common.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct LDKImage LDKImage;

  typedef struct LDKImageInfo
  {
    u32 width;
    u32 height;
    u32 channel_count;
    u64 byte_count;
    u8 const* pixels;
  } LDKImageInfo;

  LDK_API LDKImage* ldk_image_create(u32 width, u32 height, void const* pixels);
  LDK_API LDKImage* ldk_image_load(char const* path);
  LDK_API LDKImage* ldk_image_create_from_memory(void const* data, u32 data_size);
  LDK_API void ldk_image_destroy(LDKImage* image);

  LDK_API bool ldk_image_get_info(LDKImage const* image, LDKImageInfo* out_info);
  LDK_API u32 ldk_image_get_width(LDKImage const* image);
  LDK_API u32 ldk_image_get_height(LDKImage const* image);
  LDK_API u32 ldk_image_get_channel_count(LDKImage const* image);
  LDK_API u8 const* ldk_image_get_pixels(LDKImage const* image);
  LDK_API u64 ldk_image_get_byte_count(LDKImage const* image);

#ifdef __cplusplus
}
#endif

#endif
