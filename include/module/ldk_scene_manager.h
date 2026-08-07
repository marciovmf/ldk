/**
 * @file   ldk_scene_manager.h
 * @brief  Runtime scene catalog and scene switching.
 */

#ifndef LDK_SCENE_MANAGER_H
#define LDK_SCENE_MANAGER_H

#include <ldk_common.h>
#include <ldk_scene.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct LDKSceneManagerConfig
{
  const XFSPath *scenes;
  u32 scene_count;
  XFSPath runtree_path;
} LDKSceneManagerConfig;

typedef struct LDKSceneManager
{
  LDKScene *scenes;
  u32 scene_count;
  const LDKScene *current_scene;
  XFSPath runtree_path;
  bool is_initialized;
} LDKSceneManager;

// ---------------------------------------------------------------------------
// Scene Manager lifecycle
// ---------------------------------------------------------------------------

/** Initializes an empty Scene Manager. */
LDK_API bool ldk_scene_manager_initialize(LDKSceneManager *manager);

/**
 * Replaces the current game scene catalog.
 *
 * The supplied catalog is copied. Passing NULL removes the current catalog and
 * leaves the Scene Manager initialized but unconfigured.
 *
 * Replacing or removing the catalog unloads the complete current ECS scene.
 * If a new catalog cannot be copied or the ECS cannot be cleared, the previous
 * configuration remains unchanged.
 */
LDK_API bool ldk_scene_manager_override(
    LDKSceneManager *manager, const LDKSceneManagerConfig *config);

/** Unloads the current scene and frees the copied catalog. */
LDK_API void ldk_scene_manager_terminate(LDKSceneManager *manager);

// ---------------------------------------------------------------------------
// Scene catalog
// ---------------------------------------------------------------------------

LDK_API u32 ldk_scene_manager_count(const LDKSceneManager *manager);

LDK_API const LDKScene *ldk_scene_manager_at(
    const LDKSceneManager *manager, u32 index);

LDK_API const LDKScene *ldk_scene_manager_find(
    const LDKSceneManager *manager, const char *path);

// ---------------------------------------------------------------------------
// Scene loading
// ---------------------------------------------------------------------------

/** Replaces all current ECS contents with the registered scene at index. */
LDK_API const LDKScene *ldk_scene_manager_load(
    LDKSceneManager *manager, u32 index, LDKSceneResult *result);

/** Replaces all current ECS contents with a registered scene matching path. */
LDK_API const LDKScene *ldk_scene_manager_load_path(
    LDKSceneManager *manager, const char *path, LDKSceneResult *result);

/** Loads the catalog entry immediately after the current scene. */
LDK_API const LDKScene *ldk_scene_manager_load_next(
    LDKSceneManager *manager, LDKSceneResult *result);

/** Removes all current ECS contents and clears current_scene. */
LDK_API bool ldk_scene_manager_unload(LDKSceneManager *manager);

LDK_API const LDKScene *ldk_scene_manager_current(
    const LDKSceneManager *manager);

#ifdef __cplusplus
}
#endif

#endif // LDK_SCENE_MANAGER_H
