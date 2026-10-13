/**
 * # Skybox descriptors
 *
 * Represent and validate skybox image faces.
 */

#ifndef LDK_SKYBOX_H
#define LDK_SKYBOX_H

#include <ldk_asset.h>
#include <ldk_common.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @group core.skyboxes */
  typedef enum LDKSkyboxFace
  {
    LDK_SKYBOX_FACE_POSITIVE_X = 0,
    LDK_SKYBOX_FACE_NEGATIVE_X,
    LDK_SKYBOX_FACE_POSITIVE_Y,
    LDK_SKYBOX_FACE_NEGATIVE_Y,
    LDK_SKYBOX_FACE_POSITIVE_Z,
    LDK_SKYBOX_FACE_NEGATIVE_Z,
    LDK_SKYBOX_FACE_COUNT
  } LDKSkyboxFace;

  /** @group core.skyboxes */
  typedef struct LDKSkyboxDesc
  {
    LDKAssetImage faces[LDK_SKYBOX_FACE_COUNT];
  } LDKSkyboxDesc;

  /**
   * @brief Initialize defaults for skybox.
   * @param desc Description or configuration for this operation.
   * @group core.skyboxes
   */
  LDK_API void ldk_skybox_desc_defaults(LDKSkyboxDesc *desc);
  /**
   * @brief Check whether the skybox desc is valid.
   * @param desc Description or configuration for this operation.
   * @return True when the queried condition holds; false otherwise.
   * @group core.skyboxes
   */
  LDK_API bool ldk_skybox_desc_is_valid(const LDKSkyboxDesc *desc);
  /**
   * @brief Compare the skybox desc for equality.
   * @param a First value to compare or combine.
   * @param b Second value to compare or combine.
   * @return True if the operation succeeds; false otherwise.
   * @group core.skyboxes
   */
  LDK_API bool ldk_skybox_desc_equal(
      const LDKSkyboxDesc *a, const LDKSkyboxDesc *b);

#ifdef __cplusplus
}
#endif

#endif /* LDK_SKYBOX_H */
