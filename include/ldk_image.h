/**
 * # Image data
 *
 * Image pixel formats, loading, memory ownership, and pixel-buffer operations.
 */

#ifndef LDK_IMAGE_H
#define LDK_IMAGE_H

#include <ldk_common.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct LDKImage LDKImage;

  typedef enum LDKImageFormat
  {
    LDK_IMAGE_FORMAT_RGBA8_UNORM = 0,
    LDK_IMAGE_FORMAT_RGBA16_UNORM
  } LDKImageFormat;

  typedef struct LDKImageInfo
  {
    u32 width;
    u32 height;
    u32 channel_count;
    u32 bytes_per_channel;
    LDKImageFormat format;
    u64 byte_count;
    u8 const* pixels;
  } LDKImageInfo;

  /** Create an RGBA8 UNORM image.
   * @param width Width in pixels or the units expected by the API.
   * @param height Height in pixels or the units expected by the API.
   * @param pixels Image pixel data.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKImage* ldk_image_create(u32 width, u32 height, void const* pixels);

  /** Create an image from raw interleaved RGBA pixels in the requested format.
   * @param width Width in pixels or the units expected by the API.
   * @param height Height in pixels or the units expected by the API.
   * @param format Format to use for this resource.
   * @param pixels Image pixel data.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKImage* ldk_image_create_format(
      u32 width, u32 height, LDKImageFormat format, void const* pixels);

  /** Load an image file. 16-bit sources remain RGBA16 UNORM; other sources use RGBA8.
   * @param path Path identifying the file or resource.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKImage* ldk_image_load(char const* path);
  /**
   * @brief Create an image from pixel data in memory.
   * @param data Source data or system-owned context for this operation.
   * @param data_size Size of the data buffer or region.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API LDKImage* ldk_image_create_from_memory(void const* data, u32 data_size);
  /**
   * @brief Destroy the image.
   * @param image Image used by this operation.
   */
  LDK_API void ldk_image_destroy(LDKImage* image);

  /**
   * @brief Read the image format and dimension metadata.
   * @param image Image used by this operation.
   * @param out_info Receives the requested information.
   * @return True if the operation succeeds; false otherwise.
   */
  LDK_API bool ldk_image_get_info(LDKImage const* image, LDKImageInfo* out_info);
  /**
   * @brief Read the image width in pixels.
   * @param image Image used by this operation.
   * @return The resulting numeric value.
   */
  LDK_API u32 ldk_image_get_width(LDKImage const* image);
  /**
   * @brief Read the image height in pixels.
   * @param image Image used by this operation.
   * @return The resulting numeric value.
   */
  LDK_API u32 ldk_image_get_height(LDKImage const* image);
  /**
   * @brief Get the number of color channels in the image format.
   * @param image Image used by this operation.
   * @return Number of matching entries.
   */
  LDK_API u32 ldk_image_get_channel_count(LDKImage const* image);
  /**
   * @brief Query the number of bytes used per image channel.
   * @param image Image used by this operation.
   * @return The resulting numeric value.
   */
  LDK_API u32 ldk_image_get_bytes_per_channel(LDKImage const* image);
  /**
   * @brief Get the image pixel format.
   * @param image Image used by this operation.
   * @return The requested value.
   */
  LDK_API LDKImageFormat ldk_image_get_format(LDKImage const* image);
  /**
   * @brief Get the image pixel buffer.
   * @param image Image used by this operation.
   * @return Pointer to the requested object or data; NULL when unavailable.
   */
  LDK_API u8 const* ldk_image_get_pixels(LDKImage const* image);
  /**
   * @brief Get the size of the image pixel buffer in bytes.
   * @param image Image used by this operation.
   * @return Number of matching entries.
   */
  LDK_API u64 ldk_image_get_byte_count(LDKImage const* image);

#ifdef __cplusplus
}
#endif

#endif
