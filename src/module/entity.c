#include "entity/camera.h"
#include "entity/staticobject.h"
#include "maths.h"
#include <ldk/common.h>
#include <ldk/arena.h>
#include <ldk/hashmap.h>
#include <ldk/module/entity.h>
#include <ldk/hlist.h>
#include <stdint.h>

typedef struct
{
  LDKTypeId                     typeId;
  LDKEntityHandlerCreateFunc    createFunc;
  LDKEntityHandlerDestroyFunc   destroyFunc;
  LDKEntityGetTransformFunc     getTransformFunc;
  LDKEntitySetTransformFunc     setTransformFunc;
  LDKHList                      entities;
} LDKEntityHandler;

static struct
{
  LDKEntityHandler handlers[LDK_ENTITY_MANAGER_MAX_HANDLERS];
  LDKTypeId handledTypes[LDK_ENTITY_MANAGER_MAX_HANDLERS];
  uint32 numHandlers;
  uint32 numEntities;
} s_entityMgr = { 0 };

bool ldkEntityManagerInit(void)
{
  return true;
}

void ldkEntityManagerTerminate(void)
{
  for (uint32 i = 0; i < s_entityMgr.numHandlers; i++)
  {
    LDKEntityHandler* handler = &s_entityMgr.handlers[i];
    ldkHListDestroy(&handler->entities);
  }

  s_entityMgr.numHandlers = 0;
}

static LDKEntityHandler* s_entityHandlerGet(LDKTypeId typeId)
{
  for (uint32 i = 0; i < s_entityMgr.numHandlers; i++)
  {
    LDKEntityHandler* handler = &s_entityMgr.handlers[i];
    if (handler->typeId == typeId)
      return handler;
  }

  return NULL;
}

bool ldkEntityHandlerRegisterNew(LDKTypeId typeId, LDKEntityHandlerCreateFunc createFunc, LDKEntityHandlerDestroyFunc destroyFunc, LDKEntityGetTransformFunc getFunc, LDKEntitySetTransformFunc setFunc, uint32 initialPoolCapacity)
{
  LDKEntityHandler* handler = s_entityHandlerGet(typeId);
  if (handler)
  {
    ldkLogError("An entity handler for type '%s' was already registered", ldkTypeName(handler->typeId));
    return false;
  }

  if (s_entityMgr.numHandlers >= (LDK_ENTITY_MANAGER_MAX_HANDLERS - 1))
  {
    ldkLogError("Unable to register handler for type '%s'. The maximum number of handlers (%d) was reached.",
        ldkTypeName(handler->typeId), LDK_ENTITY_MANAGER_MAX_HANDLERS);
    return false;
  }

  const uint32 handlerId = s_entityMgr.numHandlers++;
  handler = &s_entityMgr.handlers[handlerId];
  handler->typeId           = typeId;
  handler->createFunc       = createFunc;
  handler->destroyFunc      = destroyFunc;
  handler->getTransformFunc = getFunc;
  handler->setTransformFunc = setFunc;
  bool success = ldkHListCreate(&handler->entities, typeId, typesize(typeId), initialPoolCapacity);
  return success;
}

LDKEntity ldkEntityManagerEntityCreate(LDKTypeId typeId)
{
  static int32 entityCount = 0;
  LDKEntityHandler* handler = s_entityHandlerGet(typeId);
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
#ifdef LDK_EDITOR
  entity->editorPlaceholder = LDK_HENTITY_INVALID;
  entity->isEditorGizmo = false;
#endif

  s_entityMgr.numEntities++;
  entityCount++;
  return entity;
}

LDKEntity ldkEntityManagerEntityLookup(LDKTypeId typeId, LDKHEntity handle)
{
  LDKEntityHandler* handler = s_entityHandlerGet(typeId);
  if (!handler)
  {
    ldkLogWarning("No entity found for handle (%s) %llx", typename(typeId), handle);
    return NULL;
  }

  return ldkHListLookup(&handler->entities,  ldkHandleFrom(handle));
}

LDKHListIterator ldkEntityManagerGetIteratorForType(LDKTypeId typeId)
{
  LDKHListIterator it;
  it.current = -1;
  it.hlist = NULL;

  LDKEntityHandler* handler = s_entityHandlerGet(typeId);
  if(handler == NULL)
    return it;

  return ldkHListIteratorCreate(&handler->entities);
}

const LDKTypeId* ldkEntityManagerTypes(uint32* count)
{
  if (count)
    *count = s_entityMgr.numHandlers;
  return s_entityMgr.handledTypes;
}

bool ldkEntityDestroy(LDKHEntity handle)
{
  LDKTypeId typeId = ldkHandleType(ldkHandleFrom(handle));
  LDKEntityHandler* handler = s_entityHandlerGet(typeId);
  if(!handler)
    return false;

  return ldkHListRemove(&handler->entities, ldkHandleFrom(handle));
}

LDKEntity ldkEntityManagerFind(LDKHEntity handle)
{
  LDKHandleType t = ldkHandleType(ldkHandleFrom(handle));
  return ldkEntityManagerEntityLookup(t, handle);
}

void ldkEntityGetTransform(LDKHEntity handle, uint32 instance, Vec3* pos, Vec3* scale, Quat* rot)
{
  LDKTypeId typeId = ldkHandleType(ldkHandleFrom(handle));
  LDKEntityHandler* handler = s_entityHandlerGet(typeId);
  LDK_ASSERT(handler != NULL);
  if (handler->getTransformFunc == NULL)
  {
    ldkLogWarning("Entity type %s has no LDKEntityEditorGetTransformFunc function registered ", typename(handler->typeId));
    return;
  }
  handler->getTransformFunc(handle, instance, pos, scale, rot);
}

void ldkEntitySetTransform(LDKHEntity handle, uint32 instance, Vec3 pos, Vec3 scale, Quat rot)
{
  LDKTypeId typeId = ldkHandleType(ldkHandleFrom(handle));
  LDKEntityHandler* handler = s_entityHandlerGet(typeId);
  LDK_ASSERT(handler != NULL);
  if (handler->setTransformFunc == NULL)
  {
    ldkLogWarning("Entity type %s has no LDKEntityEditorSetTransformFunc function registered", typename(handler->typeId));
    return;
  }
  handler->setTransformFunc(handle, instance, pos, scale, rot);
}

#ifdef LDK_DEBUG
void ldkEntityManagerPrintAll()
{
  for (uint32 i = 0; i < s_entityMgr.numHandlers; i++)
  {
    LDKEntityHandler* handler = &s_entityMgr.handlers[i];
    LDKHListIterator it = ldkHListIteratorCreate(&handler->entities);
    while(ldkHListIteratorNext(&it))
    {
      LDKEntityInfo* e = (LDKEntityInfo*) ldkHListIteratorCurrent(&it);

      if (handler->typeId == typeid(LDKCamera))
      {
        LDKCamera* o = (LDKCamera*) e;

        ldkLogInfo("Entity handle: 0x%llX, type: 0x%X, typename: %s,  name: \"%s\", active: %d, position: %f %f %f, target: %f %f %f, near: %f, far: %f, fov: %f, orthoSize: %f, type: %d",
            e->handle, e->typeId, typename(e->typeId), e->name.str, e->active, 
            o->position.x, o->position.y, o->position.z,
            o->target.x, o->target.y, o->target.z, 
            o->nearPlane, o->farPlane,
            o->fovRadian,
            o->orthoSize, o->type);

      }
      else if (handler->typeId == typeid(LDKStaticObject))
      {
        LDKStaticObject* o = (LDKStaticObject*) e;
        ldkLogInfo("Entity handle: 0x%llX, type: 0x%X, typename: %s, name: \"%s\", active: %d, position: %f %f %f, scale: %f %f %f, rotation: %f %f %f, mesh: \"%llX\"",
            e->handle, e->typeId, typename(e->typeId), e->name.str, e->active, 
            o->position.x, o->position.y, o->position.z,
            o->scale.x, o->scale.y, o->scale.z,
            o->rotation.x, o->rotation.y, o->rotation.z,
            o->mesh);
      }
      else
        ldkLogInfo("Entity handle: 0x%llX, type: 0x%X, typename: %s, name: \"%s\", active: %d", e->handle, e->typeId, typename(e->typeId), e->name.str, e->active);
    }
  }
}

#endif // LDK_DEBUG
