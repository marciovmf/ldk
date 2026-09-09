/**
 * @file   ldk_scene_manager.h
 * @brief  Runtime scene catalog and scene switching.
 */

#ifndef LDK_SCENE_MANAGER_H
#define LDK_SCENE_MANAGER_H

#include <ldk_common.h>
#include <ldk_scene.h>
#include <ldk_scene_systems.h>

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
  /* Loaded .scene associations, not catalog data. */
  LDKSceneSystems current_systems;
  u32 pending_scene_index;
  bool has_pending_scene;
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

/**
 * Replaces all current ECS contents with the registered scene at index.
 * During an engine update the request is deferred to the safe frame boundary.
 * A successful deferred request returns the target descriptor, but current()
 * continues to report the old scene until the switch completes. Failures
 * during deferred execution are logged. A failed target preflight leaves the
 * old scene running; a failure after replacement begins ends the session.
 */
LDK_API const LDKScene *ldk_scene_manager_load(
    LDKSceneManager *manager, u32 index, LDKSceneResult *result);

/** Loads a registered path, with the same deferred semantics as load(). */
LDK_API const LDKScene *ldk_scene_manager_load_path(
    LDKSceneManager *manager, const char *path, LDKSceneResult *result);

/** Loads the catalog entry immediately after the current scene. */
LDK_API const LDKScene *ldk_scene_manager_load_next(
    LDKSceneManager *manager, LDKSceneResult *result);

/** Removes all current ECS contents and clears current_scene. */
LDK_API bool ldk_scene_manager_unload(LDKSceneManager *manager);

LDK_API const LDKScene *ldk_scene_manager_current(
    const LDKSceneManager *manager);

/** The associations of the scene currently open in the ECS. */
LDK_API const LDKSceneSystems *ldk_scene_manager_systems_get(
    const LDKSceneManager *manager);

/**
 * Forgets which catalog scene owns the ECS without modifying ECS contents.
 * Use this when another loader, such as the editor direct-file loader, takes
 * ownership of the current ECS while gameplay is stopped.
 */
LDK_API bool ldk_scene_manager_current_reset(LDKSceneManager *manager);

/** Process/cancel a scene switch requested during an update callback. */
LDK_API bool ldk_scene_manager_process_pending(LDKSceneManager *manager);
LDK_API void ldk_scene_manager_pending_clear(LDKSceneManager *manager);

#ifdef __cplusplus
}
#endif

#endif // LDK_SCENE_MANAGER_H
