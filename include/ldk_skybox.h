#ifndef LDK_SKYBOX_H
#define LDK_SKYBOX_H

#include <ldk_asset.h>
#include <ldk_common.h>

#ifdef __cplusplus
extern "C"
{
#endif

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

  typedef struct LDKSkyboxDesc
  {
    LDKAssetImage faces[LDK_SKYBOX_FACE_COUNT];
  } LDKSkyboxDesc;

  LDK_API void ldk_skybox_desc_defaults(LDKSkyboxDesc *desc);
  LDK_API bool ldk_skybox_desc_is_valid(const LDKSkyboxDesc *desc);
  LDK_API bool ldk_skybox_desc_equal(
      const LDKSkyboxDesc *a, const LDKSkyboxDesc *b);

#ifdef __cplusplus
}
#endif

#endif /* LDK_SKYBOX_H */
