/**
 * # Editor callbacks
 *
 * Callbacks are registered by Comet and dispatched only while editing.
 * Annotation syntax (immediately before a function definition).
 * Prefix the following marker with two slashes and no intervening space:
 *   @editor_callback component=MyComponent flags=Manual label=Rebuild
 *   @editor_callback system=MySystem flags=OnEditorUpdate
 *   @editor_callback component=MyComponent flags=Manual,OnEditorUpdate
 * Component signature:
 *   LDK_GAME_API void function(LDKEntity entity, float dt);
 * System signature:
 *   LDK_GAME_API void function(u64 system_id, void *data, float dt);
 * For systems, system_id identifies the system in the current scene and data
 * is the scene-owned system context (NULL for systems without data).
 * Manual callbacks receive dt=0.
 * Component-scoped OnEditorUpdate callbacks run once for each matching entity;
 * system-scoped callbacks run once per editor frame only when associated with
 * the current scene.
 * The callback must not advance game simulation unless explicitly intended.
 */
#ifndef LDK_EDITOR_CALLBACK_H
#define LDK_EDITOR_CALLBACK_H

#include <ldk_common.h>
#include <module/ldk_ecs.h>

typedef enum LDKEditorCallbackFlags
{
  LDK_EDITOR_CALLBACK_MANUAL = 1u << 0,
  LDK_EDITOR_CALLBACK_ON_UPDATE = 1u << 1
} LDKEditorCallbackFlags;

typedef enum LDKEditorCallbackOwner
{
  LDK_EDITOR_CALLBACK_COMPONENT = 1,
  LDK_EDITOR_CALLBACK_SYSTEM = 2
} LDKEditorCallbackOwner;

typedef struct LDKEditorCallbackMeta
{
  const char *label;
  const char *symbol;
  LDKEditorCallbackOwner owner;
  u32 flags;
  u32 component_type;
  u64 system_id;
} LDKEditorCallbackMeta;

typedef void (*LDKEditorComponentCallbackFn)(LDKEntity entity, float dt);
typedef void (*LDKEditorSystemCallbackFn)(u64 system_id, void *data, float dt);

/* Optional game-DLL metadata entry points. No game callback is invoked by
 * the launcher or the normal ECS scheduler. */
#define LDK_GAME_EDITOR_CALLBACK_COUNT_NAME "game_editor_callback_metadata_count"
#define LDK_GAME_EDITOR_CALLBACK_GET_NAME "game_editor_callback_metadata_get"
typedef u32 (*LDKGameEditorCallbackCountFn)(void);
typedef const LDKEditorCallbackMeta *(*LDKGameEditorCallbackGetFn)(u32 index);

#endif // LDK_EDITOR_CALLBACK_H
