#include <ldk/common.h>
#include <ldk/arena.h>
#include <ldk/hashmap.h>
#include <ldk/module/entity.h>
#include <ldk/hlist.h>
#include <stdint.h>

typedef struct
{
  LDKTypeId typeId;
  LDKEntityHandlerCreateFunc createFunc;
  LDKEntityHandlerDestroyFunc destroyFunc;
  LDKHList entities;
} LDKEntityHandler;

static struct
{
  LDKEntityHandler handlers[LDK_ENTITY_MANAGER_MAX_HANDLERS];
  LDKTypeId handledTypes[LDK_ENTITY_MANAGER_MAX_HANDLERS];
  uint32 numHandlers;
  uint32 numEntities;
} internal = { 0 };

bool ldkEntityManagerInit(void)
{
  return true;
}

void ldkEntityManagerTerminate(void)
{
  for (uint32 i = 0; i < internal.numHandlers; i++)
  {
    LDKEntityHandler* handler = &internal.handlers[i];
    ldkHListDestroy(&handler->entities);
  }

  internal.numHandlers = 0;
}

static LDKEntityHandler* internalEntityHandlerGet(LDKTypeId typeId)
{
  for (uint32 i = 0; i < internal.numHandlers; i++)
  {
    LDKEntityHandler* handler = &internal.handlers[i];
    if (handler->typeId == typeId)
      return handler;
  }

  return NULL;
}

bool ldkEntityHandlerRegisterNew(LDKTypeId typeId, LDKEntityHandlerCreateFunc createFunc, LDKEntityHandlerDestroyFunc destroyFunc, uint32 initialPoolCapacity)
{
  LDKEntityHandler* handler = internalEntityHandlerGet(typeId);
  if (handler)
  {
    ldkLogError("An entity handler for type '%s' was already registered", ldkTypeName(handler->typeId));
    return false;
  }

  if (internal.numHandlers >= (LDK_ENTITY_MANAGER_MAX_HANDLERS - 1))
  {
    ldkLogError("Unable to register handler for type '%s'. The maximum number of handlers (%d) was reached.",
        ldkTypeName(handler->typeId), LDK_ENTITY_MANAGER_MAX_HANDLERS);
    return false;
  }

  const uint32 handlerId = internal.numHandlers++;
  handler = &internal.handlers[handlerId];
  handler->typeId = typeId;
  handler->createFunc = createFunc;
  handler->destroyFunc = destroyFunc;
  bool success = ldkHListCreate(&handler->entities, typeId, typesize(typeId), initialPoolCapacity);
  return success;
}

LDKEntity ldkEntityManagerEntityCreate(LDKTypeId typeId)
{
  static int32 entityCount = 0;
  LDKEntityHandler* handler = internalEntityHandlerGet(typeId);
  if (!handler)
    return NULL;
  LDKHEntity handle = ldkHandleTo(LDKHEntity, ldkHListReserve(&handler->entities));
  LDKEntityInfo* entity = (LDKEntityInfo*) ldkHListLookup(&handler->entities, ldkHandleFrom(handle));
  ldkSmallStringFormat(&entity->name, "%s_%x_%x",
      typename(handler->typeId), handler->entities.count - 1, entityCount);
  handler->createFunc(entity);
  entity->handle = handle;
  entity->flags = 0;
  entity->typeId = typeId;
  entity->active = true;

  internal.numEntities++;
  entityCount++;
  return entity;
}

LDKEntity ldkEntityManagerEntityLookup(LDKTypeId typeId, LDKHEntity handle)
{
  LDKEntityHandler* handler = internalEntityHandlerGet(typeId);
  if (!handler)
    return NULL;

  return ldkHListLookup(&handler->entities,  ldkHandleFrom(handle));
}

LDKHListIterator ldkEntityManagerGetIteratorForType(LDKTypeId typeId)
{
  LDKHListIterator it;
  it.current = -1;
  it.hlist = NULL;

  LDKEntityHandler* handler = internalEntityHandlerGet(typeId);
  if(handler == NULL)
    return it;

  return ldkHListIteratorCreate(&handler->entities);
}

const LDKTypeId* ldkEntityManagerTypes(uint32* count)
{
  if (count)
    *count = internal.numHandlers;
  return internal.handledTypes;
}

bool ldkEntityDestroy(LDKHEntity handle)
{
  LDKTypeId typeId = ldkHandleType(ldkHandleFrom(handle));
  LDKEntityHandler* handler = internalEntityHandlerGet(typeId);
  if(!handler)
    return false;

  return ldkHListRemove(&handler->entities, ldkHandleFrom(handle));
}

//TODO(marcio): Cerate means to unregister an EntityHandler (remember to fix handledTypes)
