#ifndef LDK_EDITOR_SCENE_H
#define LDK_EDITOR_SCENE_H

#include <ldk_common.h>
#include <ldk_scene.h>

#include <stdx/stdx_strbuilder.h>
#include <stdx/stdx_tml.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /*
   * Serializes the currently loaded ECS scene to TML.
   *
   * The returned TML uses temporary scene-local entity ids:
   *   - entity: 0, 1, 2...
   *   - parent: -1 for no parent, otherwise one of those scene-local ids
   *
   * Runtime LDKEntity handles are never written.
   */
  bool ldk_editor_scene_to_tml(
    XStrBuilder *out, LDKGame *game, LDKSceneResult *result);

  /*
   * Loads entities/components from a TML string into the current ECS scene.
   *
   * This function does not clear the existing scene. The caller should destroy
   * or clear existing entities first if a full replace is desired.
   */
  bool ldk_editor_scene_from_tml(
    const char *source, LDKGame *game, LDKSceneResult *result);

  bool ldk_editor_scene_save_tml_file(
    const char *path, LDKGame *game, LDKSceneResult *result);

  bool ldk_editor_scene_load_tml_file(
    const char *path, LDKGame *game, LDKSceneResult *result);

#ifdef __cplusplus
}
#endif

#endif // LDK_EDITOR_SCENE_H
