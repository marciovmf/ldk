/**
 * # Scene system configuration
 *
 * System associations stored in a scene file.
 */

#ifndef LDK_SCENE_SYSTEMS_H
#define LDK_SCENE_SYSTEMS_H

#include <ldk_scene.h>
#include <module/ldk_system.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * System associations belonging to the open scene, not to the scene catalog.
 * Initialize with {0}. The list owns its storage and must be cleared when the
 * scene is discarded. Unknown system IDs are retained.
 * grouping_ids is reserved compatibility storage, always zero.
 * Requirements belong to registered systems; scenes cannot override them.
 * data/data_sizes mirror ids by index.
 * Each non-NULL data pointer owns one zero-initialized struct whose address
 * stays stable when the association list grows. The registry only borrows it.
 */
typedef struct LDKSceneSystems
{
  u64 *ids;
  u64 *grouping_ids;
  void **data;
  u32 *data_sizes;
  u32 count;
  u32 capacity;
} LDKSceneSystems;

/** Stop and unbind owned data instances before releasing their memory.
 * Do not call while the registry is executing a callback.
 * @param systems Scene systems configuration being read or written.
 */
LDK_API void ldk_scene_systems_clear(LDKSceneSystems *systems);
/**
 * @brief Test whether the scene systems contains the requested value.
 * @param systems Scene systems configuration being read or written.
 * @param id Identifier of the target object.
 * @return True when the queried condition holds; false otherwise.
 */
LDK_API bool ldk_scene_systems_contains(const LDKSceneSystems *systems, u64 id);
/** Adds a system and allocates its data when registered.
 * @param systems Scene systems configuration being read or written.
 * @param id Identifier of the target object.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_scene_systems_add(LDKSceneSystems *systems, u64 id);
/** Deprecated: grouping_id is ignored.
 * @param systems Scene systems configuration being read or written.
 * @param id Identifier of the target object.
 * @param grouping_id Identifier of the grouping.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_scene_systems_add_with_grouping(
    LDKSceneSystems *systems, u64 id, u64 grouping_id);
/**
 * @brief Remove the scene systems.
 * @param systems Scene systems configuration being read or written.
 * @param id Identifier of the target object.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_scene_systems_remove(LDKSceneSystems *systems, u64 id);
/** Deprecated: validates membership only; grouping_id is ignored.
 * @param systems Scene systems configuration being read or written.
 * @param system_id Identifier of the system.
 * @param grouping_id Identifier of the grouping.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_scene_systems_grouping_set(
    LDKSceneSystems *systems, u64 system_id, u64 grouping_id);
/** Returns the registered system requirement hash, or zero if unavailable.
 * @param systems Scene systems configuration being read or written.
 * @param system_id Identifier of the system.
 * @return The resulting numeric value.
 */
LDK_API u64 ldk_scene_systems_grouping_get(
    const LDKSceneSystems *systems, u64 system_id);

/** Allocate zero-initialized data for registered systems. No callbacks run.
 * Existing instances keep their values; a descriptor size change is rejected.
 * @param registry Registry on which the operation is performed.
 * @param systems Scene systems configuration being read or written.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_scene_systems_prepare(
    LDKSystemRegistry *registry, LDKSceneSystems *systems);
/**
 * @brief Get the scene systems data.
 * @param systems Scene systems configuration being read or written.
 * @param system_id Identifier of the system.
 * @return Pointer to the requested object or data; NULL when unavailable.
 */
LDK_API void *ldk_scene_systems_data_get(
    const LDKSceneSystems *systems, u64 system_id);

/**
 * Reads the optional scene.systems node without creating ECS entities.
 * This is association preflight only; use ldk_scene_from_tml_with_systems
 * or ldk_scene_load_tml_file_with_systems to load fields and entity references.
 * An absent node produces an empty list. On failure, out is unchanged.
 * Accepted ids are positive signed integers or strings representing u64s.
 * The writer uses quoted hexadecimal strings to support the full u64 range.
 * Legacy grouping entries are ignored.
 *
 *   scene:
 *     systems:
 *       - id: "0x0000000000000100"
 * @param source Source object or source data for this operation.
 * @param out Output object or buffer receiving the result.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_scene_systems_from_tml(
    const char *source, LDKSceneSystems *out, LDKSceneResult *result);
/**
 * @brief Load scene system settings from a TML file.
 * @param path Path identifying the file or resource.
 * @param out Output object or buffer receiving the result.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_scene_systems_load_tml_file(
    const char *path, LDKSceneSystems *out, LDKSceneResult *result);

/**
 * Reconcile the active gameplay systems with a scene's requested ids.
 * stop_missing is called before destroying the old scene's entities;
 * start is called after the new scene's entities have been loaded.
 * Start callbacks follow registry registration order; the scene list itself
 * is an association set, not an initialization-order list.
 *
 * Scene replacement must stop ALL previous systems (pass NULL to
 * stop_missing) before clearing entities or releasing the old instances.
 * start is idempotent for an already active instance; different data restarts it.
 */
/** Validate registered system/grouping bindings without changing runtime state.
 * @param registry Registry on which the operation is performed.
 * @param systems Scene systems configuration being read or written.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_scene_systems_validate_bindings(
    LDKSystemRegistry *registry, const LDKSceneSystems *systems);
/**
 * @brief Stop systems not present in the requested scene configuration.
 * @param registry Registry on which the operation is performed.
 * @param systems Scene systems configuration being read or written.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_scene_systems_stop_missing(
    LDKSystemRegistry *registry, const LDKSceneSystems *systems);
/**
 * @brief Start the scene systems.
 * @param registry Registry on which the operation is performed.
 * @param systems Scene systems configuration being read or written.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_scene_systems_start(
    LDKSystemRegistry *registry, LDKSceneSystems *systems);

#ifdef LDK_EDITOR
/**
 * Serializes the current ECS and the supplied scene associations. The
 * existing entity serializer remains responsible for entities/components.
 * @param out Output object or buffer receiving the result.
 * @param systems Scene systems configuration being read or written.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_scene_systems_to_tml(XStrBuilder *out,
    const LDKSceneSystems *systems, LDKSceneResult *result);
/**
 * @brief Write scene system settings to a TML file.
 * @param path Path identifying the file or resource.
 * @param systems Scene systems configuration being read or written.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 */
LDK_API bool ldk_scene_systems_save_tml_file(const char *path,
    const LDKSceneSystems *systems, LDKSceneResult *result);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_SCENE_SYSTEMS_H
