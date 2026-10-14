/**
 * # Scenes and scene files
 *
 * Scene data and TML serialization.
 */

#ifndef LDK_SCENE_H
#define LDK_SCENE_H

#include <ldk_common.h>
#include <ldk_asset.h>
#include <module/ldk_entity.h>
#include <editor/ldk_component_metadata.h>

#include <stdx/stdx_filesystem.h>
#include <stdx/stdx_string.h>

#ifdef LDK_EDITOR
#include <stdx/stdx_strbuilder.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/** @group core.scenes */
typedef struct LDKGame LDKGame;
struct LDKSceneSystems;

/** @group core.scenes */
#define LDK_SCENE_TML_VERSION 1
/** @group core.scenes */
#define LDK_SCENE_NULL_ENTITY_ID (-1)

/**
 * Entry in the Scene Manager catalog.
 *
 * Scene pointers returned by the Scene Manager refer to its internal catalog
 * and remain valid until the catalog is overridden or the manager is
 * terminated.
 * @group core.scenes
 */
typedef struct LDKScene
{
  LDKAssetPath path;
  XSmallstr name;
  u32 index;
} LDKScene;

/** @group core.scenes.properties */
typedef struct LDKSceneProperties
{
  rgba32 ambient_color;
  float ambient_intensity;
} LDKSceneProperties;

/** @group core.scenes.diagnostics */
typedef struct LDKSceneResult
{
  bool ok;
  char error[256];
} LDKSceneResult;

/**
 * @brief Set default values for scene properties.
 * @param properties Scene or other property values being operated on.
 * @group core.scenes.properties
 */
LDK_API void ldk_scene_properties_defaults(LDKSceneProperties *properties);
/**
 * @brief Get the scene properties.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group core.scenes.properties
 */
LDK_API const LDKSceneProperties *ldk_scene_properties_get(void);
/**
 * @brief Set the scene properties.
 * @param properties Scene or other property values being operated on.
 * @return True if the operation succeeds; false otherwise.
 * @group core.scenes.properties
 */
LDK_API bool ldk_scene_properties_set(const LDKSceneProperties *properties);
/**
 * @brief Reset the state of the scene properties.
 * @group core.scenes.properties
 */
LDK_API void ldk_scene_properties_reset(void);

/**
 * @brief Clear the state of the scene result.
 * @param result Optional result structure populated with error details.
 * @group core.scenes.diagnostics
 */
LDK_API void ldk_scene_result_clear(LDKSceneResult *result);
/**
 * @brief Store a scene diagnostic message in the result.
 * @param result Optional result structure populated with error details.
 * @param error Receives an error description when parsing fails.
 * @group core.scenes.diagnostics
 */
LDK_API void ldk_scene_result_set_error(
    LDKSceneResult *result, const char *error);

/* Optional main-thread observer for recoverable scene-load errors. Messages
 * are also written to the engine logger. The message is valid during the call.
 * Pass NULL to unregister before destroying the observer's user data. */
/**
 * @brief Callback for reporting a scene diagnostic.
 * @param message Message displayed to the user.
 * @param user Opaque caller-supplied data passed to the callback.
 * @group core.scenes.diagnostics
 */
typedef void (*LDKSceneDiagnosticFn)(const char *message, void *user);
/**
 * @brief Set the scene diagnostic handler.
 * @param handler Handler called for the corresponding event or diagnostic.
 * @param user Opaque caller-supplied data passed to the callback.
 * @group core.scenes.diagnostics
 */
LDK_API void ldk_scene_diagnostic_handler_set(
    LDKSceneDiagnosticFn handler, void *user);

/**
 * Loads entities and components from a TML string into the current ECS.
 *
 * This function does not clear existing entities and does not roll back
 * entities created before an error. A caller performing a full scene replace
 * is responsible for clearing the ECS before the call and after a failed call.
 * @param source Source object or source data for this operation.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 * @group core.scenes.serialization.loading
 */
LDK_API bool ldk_scene_from_tml(
    const char *source, LDKSceneResult *result);

/**
 * Reads a TML file and loads it into the current ECS.
 *
 * This function has the same ECS ownership semantics as
 * ldk_scene_from_tml().
 * @param path Path identifying the file or resource.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 * @group core.scenes.serialization.loading
 */
LDK_API bool ldk_scene_load_tml_file(
    const char *path, LDKSceneResult *result);

/** Load entities and scene-owned system data together, resolving references
 * with the same entity map. Systems is initialized with {0}; on failure its
 * previous contents are unchanged. The caller owns ECS cleanup on failure,
 * and must stop old systems before replacing a scene. No initialize runs.
 * @param source Source object or source data for this operation.
 * @param systems Scene systems configuration being read or written.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 * @group core.scenes.serialization.systems
 */
LDK_API bool ldk_scene_from_tml_with_systems(const char *source,
    struct LDKSceneSystems *systems, LDKSceneResult *result);
/**
 * @brief Load scene TML file with systems.
 * @param path Path identifying the file or resource.
 * @param systems Scene systems configuration being read or written.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 * @group core.scenes.serialization.systems
 */
LDK_API bool ldk_scene_load_tml_file_with_systems(const char *path,
    struct LDKSceneSystems *systems, LDKSceneResult *result);

#ifdef LDK_EDITOR
/**
 * @brief Serialize a scene and its system configuration to TML.
 * @param out Output object or buffer receiving the result.
 * @param systems Scene systems configuration being read or written.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 * @group core.scenes.serialization.systems
 */
LDK_API bool ldk_scene_to_tml_with_systems(XStrBuilder *out,
    const struct LDKSceneSystems *systems, LDKSceneResult *result);
/** Serializes the current ECS contents to TML.
 * @param out Output object or buffer receiving the result.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 * @group core.scenes.serialization.saving
 */
LDK_API bool ldk_scene_to_tml(
    XStrBuilder *out, LDKSceneResult *result);

/** Serializes the current ECS contents and writes them to a TML file.
 * @param path Path identifying the file or resource.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 * @group core.scenes.serialization.saving
 */
LDK_API bool ldk_scene_save_tml_file(
    const char *path, LDKSceneResult *result);

/** @brief Serialize one entity hierarchy as a prefab.
 * @param root Root entity. All descendants are included.
 * @param out String builder receiving prefab TML.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 * @group core.scenes.serialization.saving
 */
LDK_API bool ldk_prefab_to_tml(
    LDKEntity root, XStrBuilder *out, LDKSceneResult *result);

/** @brief Serialize one entity hierarchy to a .prefab file.
 * @param root Root entity. All descendants are included.
 * @param path Destination filesystem path.
 * @param result Optional result structure populated with error details.
 * @return True if the operation succeeds; false otherwise.
 * @group core.scenes.serialization.saving
 */
LDK_API bool ldk_prefab_save_tml_file(
    LDKEntity root, const char *path, LDKSceneResult *result);
#endif

/**
 * @brief Resolve the runtime component type for reflected scene metadata.
 * @param meta Meta used by this operation.
 * @return The resulting numeric value.
 * @group core.scenes.metadata
 */
LDK_API u32 ldk_scene_component_meta_runtime_type(
    const LDKComponentMeta *meta);

/**
 * @brief Find serialized component metadata for a runtime type.
 * @param game Game instance associated with this operation.
 * @param component_type Registered ECS component type identifier.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group core.scenes.metadata
 */
LDK_API const LDKComponentMeta *ldk_scene_component_meta_find_by_type(
    LDKGame *game, u32 component_type);

/**
 * @brief Find the scene component field.
 * @param meta Meta used by this operation.
 * @param field_name Field name used by this operation.
 * @return Pointer to the requested object or data; NULL when unavailable.
 * @group core.scenes.metadata
 */
LDK_API const LDKComponentFieldMeta *ldk_scene_component_field_find(
    const LDKComponentMeta *meta, const char *field_name);

/**
 * @brief Check whether a component field may be saved in a scene.
 * @param meta Meta used by this operation.
 * @param field Field used by this operation.
 * @return True when the queried condition holds; false otherwise.
 * @group core.scenes.metadata
 */
LDK_API bool ldk_scene_component_field_is_serializable(
    const LDKComponentMeta *meta, const LDKComponentFieldMeta *field);

#ifdef __cplusplus
}
#endif

#endif // LDK_SCENE_H
