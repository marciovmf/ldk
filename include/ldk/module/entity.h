/**
 * entity.h
 * 
 * A simple entity manager for LDK.
 * An entity is any piece of persistent data relevant for the game.
 */

#ifndef LDK_ENTITY_H
#define LDK_ENTITY_H

#include "../common.h"
#include "../hlist.h"
#include "../maths.h"

#ifndef LDK_ENTITY_MANAGER_MAX_HANDLERS
#define LDK_ENTITY_MANAGER_MAX_HANDLERS 32
#endif

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct
  {
    bool active;
    LDKSmallStr name;
    LDKTypeId   typeId;     // The type of the entity
    LDKHEntity  handle;     // The unique entity handle for this entity.
    uint32      flags;      // Space for user defined flags
#ifdef LDK_EDITOR
    uint32      isEditorGizmo;     // Renderer won't highlight gizmo objects
    LDKHEntity  editorPlaceholder; // this is used by the editor if a placeholder is necessary
#endif                                 
  } LDKEntityInfo;

  /*
   * This macro must be included inside the declaration of an entity structure
   * to ensure the entity contains an LDKEntityInfo struct;
   */
#define LDK_DECLARE_ENTITY LDKEntityInfo entity

#define ldkEntityTypeRegister(type, createFunc, destroyFunc, getTransformFunc, setTransformFunc, capacity) ldkEntityHandlerRegisterNew(typeid(type), (LDKEntityHandlerCreateFunc) createFunc,  (LDKEntityHandlerDestroyFunc)  destroyFunc, getTransformFunc, setTransformFunc, capacity)

#define ldkEntityCreate(e) ((e*) ldkEntityManagerEntityCreate(typeid(e)))

#define ldkEntityLookup(type, handle) ((type*) ldkEntityManagerEntityLookup(typeid(type), handle))

#define ldkEntityManagerGetIterator(type) ldkEntityManagerGetIteratorForType(typeid(type))

#define ldkEntitySetNameFormat(e, fmt, ...) ldkSmallStringFormat(&e->entity.name, fmt, __VA_ARGS__)

#define ldkEntitySetName(e, n) ldkSmallString(&e->entity.name, n)

  typedef void* LDKEntity;
  typedef void (*LDKEntityHandlerCreateFunc) (LDKEntity);
  typedef void (*LDKEntityHandlerDestroyFunc) (LDKEntity);
  typedef void (*LDKEntityGetTransformFunc) (LDKHEntity, uint32, Vec3*, Vec3*, Quat*);
  typedef void (*LDKEntitySetTransformFunc) (LDKHEntity, uint32, Vec3, Vec3, Quat);

  LDK_API bool ldkEntityManagerInit(void);
  LDK_API void ldkEntityManagerTerminate(void);
  LDK_API bool ldkEntityHandlerRegisterNew(LDKTypeId typeId, LDKEntityHandlerCreateFunc createFunc, LDKEntityHandlerDestroyFunc destroyFunc, LDKEntityGetTransformFunc getFunc, LDKEntitySetTransformFunc setFunc, uint32 initialPoolCapacity);
  LDK_API LDKEntity ldkEntityManagerEntityCreate(LDKTypeId typeId);
  LDK_API LDKEntity ldkEntityManagerEntityLookup(LDKTypeId typeId, LDKHEntity handle);
  LDK_API LDKEntity ldkEntityManagerEntitiesGet(LDKTypeId typeId, uint32* count);
  LDK_API const LDKTypeId* ldkEntityManagerTypes(uint32* count);
  LDK_API bool ldkEntityDestroy(LDKHEntity handle);
  LDK_API LDKHListIterator ldkEntityManagerGetIteratorForType(LDKTypeId typeId);
  LDK_API LDKEntity ldkEntityManagerFind(LDKHEntity handle);

#ifdef LDK_EDITOR

  typedef struct
  {
    LDKHEntity handle;
    uint32 surfaceIndex;
    uint32 instanceIndex;
  } LDKEntitySelectionInfo;

  LDK_API void ldkEntityHandlerRegisterFunctions(LDKTypeId typeId,
      LDKEntityGetTransformFunc getFunc, LDKEntitySetTransformFunc setFunc);

#ifdef LDK_EDITOR
  LDK_API void ldkEntityGetTransform(LDKHEntity handle, uint32 instance, Vec3* pos, Vec3* scale, Quat* rot);
  LDK_API void ldkEntitySetTransform(LDKHEntity handle, uint32 instance, Vec3 pos, Vec3 scale, Quat rot);
#endif // LDK_EDITOR

#endif

  LDK_API void ldkEntityManagerPrintAll();

#ifdef __cplusplus
}
#endif

#endif //LDK_ENTITY_H

