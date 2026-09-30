#include <ldk_skybox.h>

#include <string.h>

void ldk_skybox_desc_defaults(LDKSkyboxDesc *desc)
{
  if (!desc)
  {
    return;
  }

  memset(desc, 0, sizeof(*desc));
  for (u32 i = 0; i < LDK_SKYBOX_FACE_COUNT; ++i)
  {
    desc->faces[i].h = x_handle_null();
  }
}

bool ldk_skybox_desc_is_valid(const LDKSkyboxDesc *desc)
{
  if (!desc)
  {
    return false;
  }

  for (u32 i = 0; i < LDK_SKYBOX_FACE_COUNT; ++i)
  {
    if (x_handle_is_null(desc->faces[i].h))
    {
      return false;
    }
  }

  return true;
}

bool ldk_skybox_desc_equal(const LDKSkyboxDesc *a, const LDKSkyboxDesc *b)
{
  if (!a || !b)
  {
    return false;
  }

  for (u32 i = 0; i < LDK_SKYBOX_FACE_COUNT; ++i)
  {
    if (a->faces[i].h.index != b->faces[i].h.index ||
        a->faces[i].h.version != b->faces[i].h.version)
    {
      return false;
    }
  }

  return true;
}
