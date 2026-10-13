/**
 * # Scene manager
 *
 * Runtime scene catalog and scene switching.
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

/** @group modules.scene_manager.lifecycle */
typedef struct LDKSceneManagerConfig
{
  const LDKAssetPath *scenes;
  u32 scene_count;
  /* Optional friendly names, parallel to scenes. Empty names use the filename.
   */
  const XSmallstr *names;
} LDKSceneManagerConfig;

/** @group modules.scene_manager.lifecycle */
typedef struct LDKSceneManager
{
  LDKScene *scenes;
  u32 scene_count;
  const LDKScene *current_scene;
  /* Loaded .scene associations, not catalog data. */
  LDKSceneSystems current_systems;
  u32 pending_scene_index;
  bool has_pending_scene;
  bool is_initialized;
} LDKSceneManager;

// ---------------------------------------------------------------------------
// Scene Manager lifecycle
// ---------------------------------------------------------------------------

/** Initializes an empty Scene Manager.
 * @param manager Asset manager that owns or resolves the asset.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.scene_manager.lifecycle
 */
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
 * @param manager Asset manager that owns or resolves the asset.
 * @param config Configuration for this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.scene_manager.lifecycle
 */
LDK_API bool ldk_scene_manager_override(
    LDKSceneManager *manager, const LDKSceneManagerConfig *config);

/** Validate editable catalog entries without loading any scene.
 * @param scenes Scenes used by this operation.
 * @param count Number of items to process.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.scene_manager.lifecycle
 */
LDK_API bool ldk_scene_manager_catalog_validate(
    const LDKScene *scenes, u32 count);

/**
 * Exchange ownership of catalog arrays while stopped, without touching ECS,
 * systems or pending scene contents. The caller receives the old catalog and
 * must free it. The current scene is remapped by path, or detached if removed.
 * No allocation is performed. On failure neither array changes ownership.
 * @param manager Asset manager that owns or resolves the asset.
 * @param scenes Scenes used by this operation.
 * @param count Number of items to process.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.scene_manager.lifecycle
 */
LDK_API bool ldk_scene_manager_catalog_exchange(
    LDKSceneManager *manager, LDKScene **scenes, u32 *count);

/**
 * Reads [scenes] from a project manifest or runtime INI.
 * count=N, 0.path=relative.scene, 0.name=Friendly name, ...
 * Indices are contiguous from zero; names are optional. Paths are logical
 * asset paths. An absent/empty catalog configures no scenes. On parse failure
 * the existing catalog and scene remain unchanged.
 * @param manager Asset manager that owns or resolves the asset.
 * @param ini_path Path to the INI configuration.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.scene_manager.lifecycle
 */
LDK_API bool ldk_scene_manager_configure_file(LDKSceneManager *manager,
    const char *ini_path, LDKSceneResult *result);

/** Unloads the current scene and frees the copied catalog.
 * @param manager Asset manager that owns or resolves the asset.
 * @group modules.scene_manager.lifecycle
 */
LDK_API void ldk_scene_manager_terminate(LDKSceneManager *manager);

// ---------------------------------------------------------------------------
// Scene catalog
// ---------------------------------------------------------------------------

/**
 * @brief Get the number of scenes in the scene manager.
 * @param manager Asset manager that owns or resolves the asset.
 * @return Number of matching entries.
 * @group modules.scene_manager.lifecycle
 */
LDK_API u32 ldk_scene_manager_count(const LDKSceneManager *manager);

/**
 * @brief Get a scene by its index in the scene manager.
 * @param manager Asset manager that owns or resolves the asset.
 * @param index Zero-based index of the selected item.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.scene_manager.lifecycle
 */
LDK_API const LDKScene *ldk_scene_manager_at(
    const LDKSceneManager *manager, u32 index);

/**
 * @brief Find a scene registered with the manager.
 * @param manager Asset manager that owns or resolves the asset.
 * @param path Path identifying the file or resource.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.scene_manager.lifecycle
 */
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
 * @param manager Asset manager that owns or resolves the asset.
 * @param index Zero-based index of the selected item.
 * @param result Optional result structure populated with error details.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.scene_manager.loading
 */
LDK_API const LDKScene *ldk_scene_manager_load(
    LDKSceneManager *manager, u32 index, LDKSceneResult *result);

/** Loads a registered path, with the same deferred semantics as load().
 * @param manager Asset manager that owns or resolves the asset.
 * @param path Path identifying the file or resource.
 * @param result Optional result structure populated with error details.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.scene_manager.loading
 */
LDK_API const LDKScene *ldk_scene_manager_load_path(
    LDKSceneManager *manager, const char *path, LDKSceneResult *result);

/** Loads the catalog entry immediately after the current scene.
 * @param manager Asset manager that owns or resolves the asset.
 * @param result Optional result structure populated with error details.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.scene_manager.loading
 */
LDK_API const LDKScene *ldk_scene_manager_load_next(
    LDKSceneManager *manager, LDKSceneResult *result);

/** Removes all current ECS contents and clears current_scene.
 * @param manager Asset manager that owns or resolves the asset.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.scene_manager.loading
 */
LDK_API bool ldk_scene_manager_unload(LDKSceneManager *manager);

/**
 * @brief Get the currently active scene.
 * @param manager Asset manager that owns or resolves the asset.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.scene_manager.active_scene
 */
LDK_API const LDKScene *ldk_scene_manager_current(
    const LDKSceneManager *manager);

/** The associations of the scene currently open in the ECS.
 * @param manager Asset manager that owns or resolves the asset.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group modules.scene_manager.lifecycle
 */
LDK_API const LDKSceneSystems *ldk_scene_manager_systems_get(
    const LDKSceneManager *manager);

/**
 * Forgets which catalog scene owns the ECS without modifying ECS contents.
 * Use this when another loader, such as the editor direct-file loader, takes
 * ownership of the current ECS while gameplay is stopped.
 * @param manager Asset manager that owns or resolves the asset.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.scene_manager.active_scene
 */
LDK_API bool ldk_scene_manager_current_reset(LDKSceneManager *manager);

/** Process/cancel a scene switch requested during an update callback.
 * @param manager Asset manager that owns or resolves the asset.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.scene_manager.lifecycle
 */
LDK_API bool ldk_scene_manager_process_pending(LDKSceneManager *manager);
/**
 * @brief Clear the state of the scene manager pending.
 * @param manager Asset manager that owns or resolves the asset.
 * @group modules.scene_manager.lifecycle
 */
LDK_API void ldk_scene_manager_pending_clear(LDKSceneManager *manager);

#ifdef __cplusplus
}
#endif

#endif // LDK_SCENE_MANAGER_H
