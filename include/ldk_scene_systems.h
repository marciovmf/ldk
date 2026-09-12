/**
 * @file   ldk_scene_systems.h
 * @brief  System associations stored in a scene file.
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
 * scene is discarded. Unknown system and grouping ids are retained.
 *
 * grouping_ids mirrors ids by index. A grouping id of 0 means the system is
 * not bound to an entity grouping. Keeping ids as its own array preserves the
 * existing scene-system API and storage semantics for callers that only care
 * about the selected systems. data/data_sizes mirror the same indices.
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
 * Do not call while the registry is executing a callback. */
LDK_API void ldk_scene_systems_clear(LDKSceneSystems *systems);
LDK_API bool ldk_scene_systems_contains(const LDKSceneSystems *systems, u64 id);
/** Adds an unbound system and allocates its data when registered. */
LDK_API bool ldk_scene_systems_add(LDKSceneSystems *systems, u64 id);
LDK_API bool ldk_scene_systems_add_with_grouping(
    LDKSceneSystems *systems, u64 id, u64 grouping_id);
LDK_API bool ldk_scene_systems_remove(LDKSceneSystems *systems, u64 id);
LDK_API bool ldk_scene_systems_grouping_set(
    LDKSceneSystems *systems, u64 system_id, u64 grouping_id);
LDK_API u64 ldk_scene_systems_grouping_get(
    const LDKSceneSystems *systems, u64 system_id);

/** Allocate zero-initialized data for registered systems. No callbacks run.
 * Existing instances keep their values; a descriptor size change is rejected. */
LDK_API bool ldk_scene_systems_prepare(
    LDKSystemRegistry *registry, LDKSceneSystems *systems);
LDK_API void *ldk_scene_systems_data_get(
    const LDKSceneSystems *systems, u64 system_id);

/**
 * Reads the optional scene.systems node without creating ECS entities.
 * This is association preflight only; use ldk_scene_from_tml_with_systems
 * or ldk_scene_load_tml_file_with_systems to load fields and entity references.
 * An absent node produces an empty list. On failure, out is unchanged.
 * Accepted ids are positive signed integers or strings representing u64s.
 * The writer uses quoted hexadecimal strings to support the full u64 range.
 * The optional grouping entry defaults to 0 for old scene files.
 *
 *   scene:
 *     systems:
 *       - id: "0x0000000000000100"
 *         grouping: "0x0000000000000200"
 */
LDK_API bool ldk_scene_systems_from_tml(
    const char *source, LDKSceneSystems *out, LDKSceneResult *result);
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
/** Validate registered system/grouping bindings without changing runtime state. */
LDK_API bool ldk_scene_systems_validate_bindings(
    LDKSystemRegistry *registry, const LDKSceneSystems *systems);
LDK_API bool ldk_scene_systems_stop_missing(
    LDKSystemRegistry *registry, const LDKSceneSystems *systems);
LDK_API bool ldk_scene_systems_start(
    LDKSystemRegistry *registry, LDKSceneSystems *systems);

#ifdef LDK_EDITOR
/**
 * Serializes the current ECS and the supplied scene associations. The
 * existing entity serializer remains responsible for entities/components.
 */
LDK_API bool ldk_scene_systems_to_tml(XStrBuilder *out,
    const LDKSceneSystems *systems, LDKSceneResult *result);
LDK_API bool ldk_scene_systems_save_tml_file(const char *path,
    const LDKSceneSystems *systems, LDKSceneResult *result);
#endif

#ifdef __cplusplus
}
#endif

#endif // LDK_SCENE_SYSTEMS_H
