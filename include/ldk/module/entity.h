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
    LDKTypeId typeId;     // The type of the entity
    uint32 flags;         // Space for user defined flags
    LDKHandle handle;     // The unique entity handle for this entity.
  } LDKEntityInfo;

  /*
   * This macro must be included inside the declaration of an entity structure
   * to ensure the entity contains an LDKEntityInfo struct;
   */
#define LDK_DECLARE_ENTITY LDKEntityInfo entity


#define ldkEntityTypeRegister(type, createFunc, destroyFunc, capacity) ldkEntityHandlerRegisterNew(typeid(type), (LDKEntityHandlerCreateFunc) createFunc,  (LDKEntityHandlerDestroyFunc)  destroyFunc, capacity)

#define ldkEntityCreate(e) ((e*) ldkEntityManagerEntityCreate(typeid(e)))

#define ldkEntityLookup(type, handle) ((type*) ldkEntityManagerEntityLookup(typeid(type), handle))

#define ldkEntityManagerGetIterator(type) ldkEntityManagerGetIteratorForType(typeid(type))


  typedef void* LDKEntity;
  typedef void (*LDKEntityHandlerCreateFunc) (LDKEntity);
  typedef void (*LDKEntityHandlerDestroyFunc) (LDKEntity);


  LDK_API bool ldkEntityManagerInit(void);
  LDK_API void ldkEntityManagerTerminate(void);
  LDK_API bool ldkEntityHandlerRegisterNew(LDKTypeId typeId, LDKEntityHandlerCreateFunc createFunc, LDKEntityHandlerDestroyFunc destroyFunc, uint32 initialPoolCapacity);
  LDK_API LDKEntity ldkEntityManagerEntityCreate(LDKTypeId typeId);
  LDK_API LDKEntity ldkEntityManagerEntityLookup(LDKTypeId typeId, LDKHandle handle);
  LDK_API LDKEntity ldkEntityManagerEntitiesGet(LDKTypeId typeId, uint32* count);
  LDK_API const LDKTypeId* ldkEntityManagerTypes(uint32* count);
  LDK_API bool ldkEntityDestroy(LDKHandle handle);
  LDK_API LDKHListIterator ldkEntityManagerGetIteratorForType(LDKTypeId typeId);

#ifdef __cplusplus
}
#endif

#endif //LDK_ENTITY_H

