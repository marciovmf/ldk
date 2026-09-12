#include "component/ldk_mesh_source.h"
#include <module/ldk_system.h>
#include <module/ldk_ecs.h>
#include <module/ldk_entity.h>
#include <component/ldk_transform.h>
#include <component/ldk_camera.h>
#include <ldk.h>

#include <stdx/stdx_hashtable.h>
#include <stdx/stdx_ini.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef LDK_DEFAULT_TRANSFORM_COUNT
#define LDK_DEFAULT_TRANSFORM_COUNT 64
#endif

#ifndef LDK_DEFAULT_CAMERA_COUNT
#define LDK_DEFAULT_CAMERA_COUNT 4
#endif

#ifndef LDK_DEFAULT_MESHSOURCE_COUNT
#define LDK_DEFAULT_MESHSOURCE_COUNT 4
#endif

X_HASHTABLE_TYPE_NAMED(u64, u32, grouping_slot);

typedef struct LDKRegisteredGrouping
{
  LDKGroupingDesc desc;
  char *name;
  u32 *component_types;
  LDKEntity *entities;
  u32 entity_count;
  u32 entity_capacity;
  XHashtable_grouping_slot *slots;
  LDKEntityGroup view;
  bool config_defined;
} LDKRegisteredGrouping;

typedef struct LDKGroupingRegistryInternal
{
  LDKRegisteredGrouping **groupings;
  u32 count;
  u32 capacity;
  LDKEntity *dirty_entities;
  u32 dirty_count;
  u32 dirty_capacity;
  XHashtable_grouping_slot *dirty_set;
  bool rebuild_pending;
} LDKGroupingRegistryInternal;

typedef struct LDKGroupingBuildContext
{
  LDKECS *ecs;
  LDKRegisteredGrouping *grouping;
  bool ok;
} LDKGroupingBuildContext;

typedef struct LDKGroupingConfigEntry
{
  u64 id;
  char *name;
  u32 *component_types;
  u32 component_count;
} LDKGroupingConfigEntry;


static LDKECS *s_ecs(void)
{
  return (LDKECS *)ldk_module_get(LDK_MODULE_ECS);
}

static LDKGroupingRegistryInternal *s_grouping_internal(LDKECS *ecs)
{
  return ecs ? (LDKGroupingRegistryInternal *)ecs->grouping_internal : NULL;
}

static u64 s_entity_key(LDKEntity entity)
{
  return ((u64)entity.version << 32u) | (u64)entity.index;
}

static char *s_string_copy(const char *source)
{
  char *copy;
  size_t length;

  if (!source)
  {
    return NULL;
  }

  length = strlen(source);
  if (length == SIZE_MAX)
  {
    return NULL;
  }

  copy = (char *)malloc(length + 1u);
  if (!copy)
  {
    return NULL;
  }

  memcpy(copy, source, length + 1u);
  return copy;
}

static bool s_grouping_registry_reserve(
    LDKGroupingRegistryInternal *internal, u32 min_capacity)
{
  LDKRegisteredGrouping **groupings;
  u32 capacity;

  if (!internal)
  {
    return false;
  }

  if (internal->capacity >= min_capacity)
  {
    return true;
  }

  capacity = internal->capacity ? internal->capacity : 8u;
  if (capacity < min_capacity && capacity <= UINT32_MAX / 2u)
  {
    capacity *= 2u;
  }
  while (capacity < min_capacity)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = min_capacity;
      break;
    }
    capacity *= 2u;
  }

  if ((size_t)capacity > SIZE_MAX / sizeof(*groupings))
  {
    return false;
  }

  groupings = (LDKRegisteredGrouping **)realloc(
      internal->groupings, sizeof(*groupings) * (size_t)capacity);
  if (!groupings)
  {
    return false;
  }

  internal->groupings = groupings;
  internal->capacity = capacity;
  return true;
}

static bool s_grouping_entities_reserve(
    LDKRegisteredGrouping *grouping, u32 min_capacity)
{
  LDKEntity *entities;
  u32 capacity;

  if (!grouping)
  {
    return false;
  }

  if (grouping->entity_capacity >= min_capacity)
  {
    return true;
  }

  capacity = grouping->entity_capacity ? grouping->entity_capacity : 32u;
  if (capacity < min_capacity && capacity <= UINT32_MAX / 2u)
  {
    capacity *= 2u;
  }
  while (capacity < min_capacity)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = min_capacity;
      break;
    }
    capacity *= 2u;
  }

  if ((size_t)capacity > SIZE_MAX / sizeof(*entities))
  {
    return false;
  }

  entities = (LDKEntity *)realloc(
      grouping->entities, sizeof(*entities) * (size_t)capacity);
  if (!entities)
  {
    return false;
  }

  grouping->entities = entities;
  grouping->entity_capacity = capacity;
  grouping->view.entities = grouping->entities;
  return true;
}

static bool s_grouping_dirty_reserve(
    LDKGroupingRegistryInternal *internal, u32 min_capacity)
{
  LDKEntity *entities;
  u32 capacity;

  if (!internal)
  {
    return false;
  }

  if (internal->dirty_capacity >= min_capacity)
  {
    return true;
  }

  capacity = internal->dirty_capacity ? internal->dirty_capacity : 64u;
  if (capacity < min_capacity && capacity <= UINT32_MAX / 2u)
  {
    capacity *= 2u;
  }
  while (capacity < min_capacity)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = min_capacity;
      break;
    }
    capacity *= 2u;
  }

  if ((size_t)capacity > SIZE_MAX / sizeof(*entities))
  {
    return false;
  }

  entities = (LDKEntity *)realloc(
      internal->dirty_entities, sizeof(*entities) * (size_t)capacity);
  if (!entities)
  {
    return false;
  }

  internal->dirty_entities = entities;
  internal->dirty_capacity = capacity;
  return true;
}

static LDKRegisteredGrouping *s_grouping_find(
    LDKGroupingRegistryInternal *internal, u64 id)
{
  if (!internal || id == 0)
  {
    return NULL;
  }

  for (u32 i = 0; i < internal->count; ++i)
  {
    if (internal->groupings[i]->desc.id == id)
    {
      return internal->groupings[i];
    }
  }

  return NULL;
}

static const LDKRegisteredGrouping *s_grouping_find_const(
    const LDKGroupingRegistryInternal *internal, u64 id)
{
  return s_grouping_find((LDKGroupingRegistryInternal *)internal, id);
}

static bool s_grouping_name_exists(
    const LDKGroupingRegistryInternal *internal, const char *name,
    bool ignore_config_defined)
{
  if (!internal || !name)
  {
    return false;
  }

  for (u32 i = 0; i < internal->count; ++i)
  {
    const LDKRegisteredGrouping *grouping = internal->groupings[i];
    if (ignore_config_defined && grouping->config_defined)
    {
      continue;
    }
    if (strcmp(grouping->desc.name, name) == 0)
    {
      return true;
    }
  }

  return false;
}

static bool s_grouping_desc_is_valid(LDKECS *ecs, const LDKGroupingDesc *desc,
    bool ignore_config_defined)
{
  LDKGroupingRegistryInternal *internal = s_grouping_internal(ecs);

  if (!ecs || !internal || !desc || desc->id == 0 || !desc->name ||
      !desc->name[0] || desc->component_count > LDK_ENTITY_MAX_COMPONENTS ||
      (desc->component_count && !desc->component_types))
  {
    return false;
  }

  for (u32 i = 0; i < internal->count; ++i)
  {
    const LDKRegisteredGrouping *grouping = internal->groupings[i];
    if (ignore_config_defined && grouping->config_defined)
    {
      continue;
    }
    if (grouping->desc.id == desc->id)
    {
      return false;
    }
  }

  if (s_grouping_name_exists(internal, desc->name, ignore_config_defined))
  {
    return false;
  }

  for (u32 i = 0; i < desc->component_count; ++i)
  {
    if (desc->component_types[i] == 0 ||
        !ldk_component_is_registered(&ecs->component, desc->component_types[i]))
    {
      return false;
    }

    for (u32 j = 0; j < i; ++j)
    {
      if (desc->component_types[j] == desc->component_types[i])
      {
        return false;
      }
    }
  }

  return true;
}

static bool s_grouping_matches_entity(
    LDKECS *ecs, const LDKRegisteredGrouping *grouping, LDKEntity entity)
{
  if (!ecs || !grouping || !ldk_entity_is_alive(&ecs->entity, entity) ||
      ldk_entity_internal_flags_has(
          &ecs->entity, entity, LDK_ENTITY_INTERNAL_EDITOR))
  {
    return false;
  }

  for (u32 i = 0; i < grouping->desc.component_count; ++i)
  {
    if (!ldk_entity_component_has(
            &ecs->entity, entity, grouping->desc.component_types[i]))
    {
      return false;
    }
  }

  return true;
}

static bool s_grouping_add_entity(
    LDKRegisteredGrouping *grouping, LDKEntity entity)
{
  u64 key;
  u32 slot;

  if (!grouping || !grouping->slots)
  {
    return false;
  }

  key = s_entity_key(entity);
  if (x_hashtable_grouping_slot_has(grouping->slots, key))
  {
    return true;
  }

  if (!s_grouping_entities_reserve(grouping, grouping->entity_count + 1u))
  {
    return false;
  }

  slot = grouping->entity_count;
  grouping->entities[slot] = entity;
  if (!x_hashtable_grouping_slot_set(grouping->slots, key, slot))
  {
    return false;
  }

  grouping->entity_count += 1u;
  grouping->view.entities = grouping->entities;
  grouping->view.count = grouping->entity_count;
  return true;
}

static bool s_grouping_remove_entity(
    LDKRegisteredGrouping *grouping, LDKEntity entity)
{
  u64 key;
  u32 slot;
  u32 last;

  if (!grouping || !grouping->slots)
  {
    return false;
  }

  key = s_entity_key(entity);
  if (!x_hashtable_grouping_slot_get(grouping->slots, key, &slot))
  {
    return true;
  }

  if (slot >= grouping->entity_count)
  {
    return false;
  }

  last = grouping->entity_count - 1u;
  if (slot != last)
  {
    LDKEntity moved = grouping->entities[last];
    grouping->entities[slot] = moved;
    if (!x_hashtable_grouping_slot_set(
            grouping->slots, s_entity_key(moved), slot))
    {
      return false;
    }
  }

  if (!x_hashtable_grouping_slot_remove(grouping->slots, key))
  {
    return false;
  }

  grouping->entity_count = last;
  grouping->view.count = grouping->entity_count;
  return true;
}

static bool s_grouping_sync_entity(
    LDKECS *ecs, LDKRegisteredGrouping *grouping, LDKEntity entity)
{
  if (s_grouping_matches_entity(ecs, grouping, entity))
  {
    return s_grouping_add_entity(grouping, entity);
  }
  return s_grouping_remove_entity(grouping, entity);
}

static bool s_grouping_build_entity(
    LDKEntity entity, LDKEntityInfo *info, void *user)
{
  LDKGroupingBuildContext *context = (LDKGroupingBuildContext *)user;
  (void)info;

  if (!context || !context->ok)
  {
    return false;
  }

  if (!s_grouping_sync_entity(context->ecs, context->grouping, entity))
  {
    context->ok = false;
    return false;
  }

  return true;
}

static void s_grouping_clear_membership(LDKRegisteredGrouping *grouping)
{
  if (!grouping || !grouping->slots)
  {
    return;
  }

  for (u32 i = 0; i < grouping->entity_count; ++i)
  {
    x_hashtable_grouping_slot_remove(
        grouping->slots, s_entity_key(grouping->entities[i]));
  }

  grouping->entity_count = 0;
  grouping->view.count = 0;
}

static bool s_grouping_rebuild(
    LDKECS *ecs, LDKRegisteredGrouping *grouping)
{
  LDKGroupingBuildContext context;

  if (!ecs || !grouping)
  {
    return false;
  }

  s_grouping_clear_membership(grouping);
  context.ecs = ecs;
  context.grouping = grouping;
  context.ok = true;
  ldk_entity_foreach(&ecs->entity, s_grouping_build_entity, &context);
  return context.ok;
}

static void s_grouping_destroy(LDKRegisteredGrouping *grouping)
{
  if (!grouping)
  {
    return;
  }

  x_hashtable_grouping_slot_destroy(grouping->slots);
  free(grouping->entities);
  free(grouping->component_types);
  free(grouping->name);
  free(grouping);
}

static LDKRegisteredGrouping *s_grouping_create(
    LDKECS *ecs, const LDKGroupingDesc *desc, bool config_defined)
{
  LDKRegisteredGrouping *grouping;

  if (!ecs || !desc)
  {
    return NULL;
  }

  grouping = (LDKRegisteredGrouping *)calloc(1, sizeof(*grouping));
  if (!grouping)
  {
    return NULL;
  }

  grouping->name = s_string_copy(desc->name);
  grouping->slots = x_hashtable_grouping_slot_create();
  if (!grouping->name || !grouping->slots)
  {
    s_grouping_destroy(grouping);
    return NULL;
  }

  if (desc->component_count)
  {
    grouping->component_types =
        (u32 *)malloc(sizeof(u32) * (size_t)desc->component_count);
    if (!grouping->component_types)
    {
      s_grouping_destroy(grouping);
      return NULL;
    }
    memcpy(grouping->component_types, desc->component_types,
        sizeof(u32) * (size_t)desc->component_count);
  }

  grouping->desc.id = desc->id;
  grouping->desc.name = grouping->name;
  grouping->desc.component_types = grouping->component_types;
  grouping->desc.component_count = desc->component_count;
  grouping->view.grouping_id = desc->id;
  grouping->config_defined = config_defined;

  if (!s_grouping_rebuild(ecs, grouping))
  {
    s_grouping_destroy(grouping);
    return NULL;
  }

  return grouping;
}

static bool s_grouping_registry_initialize(LDKECS *ecs)
{
  LDKGroupingRegistryInternal *internal;

  if (!ecs || ecs->grouping_internal)
  {
    return false;
  }

  internal = (LDKGroupingRegistryInternal *)calloc(1, sizeof(*internal));
  if (!internal)
  {
    return false;
  }

  internal->dirty_set = x_hashtable_grouping_slot_create();
  if (!internal->dirty_set)
  {
    free(internal);
    return false;
  }

  ecs->grouping_internal = internal;
  return true;
}

static void s_grouping_registry_terminate(LDKECS *ecs)
{
  LDKGroupingRegistryInternal *internal = s_grouping_internal(ecs);

  if (!internal)
  {
    return;
  }

  for (u32 i = 0; i < internal->count; ++i)
  {
    s_grouping_destroy(internal->groupings[i]);
  }

  x_hashtable_grouping_slot_destroy(internal->dirty_set);
  free(internal->dirty_entities);
  free(internal->groupings);
  free(internal);
  ecs->grouping_internal = NULL;
}

static void s_grouping_entity_dirty(LDKECS *ecs, LDKEntity entity)
{
  LDKGroupingRegistryInternal *internal = s_grouping_internal(ecs);
  u64 key;

  if (!internal)
  {
    return;
  }

  key = s_entity_key(entity);
  if (x_hashtable_grouping_slot_has(internal->dirty_set, key))
  {
    return;
  }

  if (!s_grouping_dirty_reserve(internal, internal->dirty_count + 1u) ||
      !x_hashtable_grouping_slot_set(
          internal->dirty_set, key, internal->dirty_count))
  {
    internal->rebuild_pending = true;
    return;
  }

  internal->dirty_entities[internal->dirty_count++] = entity;
}

static bool s_grouping_flush(LDKECS *ecs)
{
  LDKGroupingRegistryInternal *internal = s_grouping_internal(ecs);
  bool rebuild;

  if (!ecs || !internal)
  {
    return false;
  }

  rebuild = internal->rebuild_pending;
  if (!rebuild)
  {
    for (u32 dirty_index = 0; dirty_index < internal->dirty_count;
         ++dirty_index)
    {
      LDKEntity entity = internal->dirty_entities[dirty_index];
      for (u32 grouping_index = 0; grouping_index < internal->count;
           ++grouping_index)
      {
        if (!s_grouping_sync_entity(
                ecs, internal->groupings[grouping_index], entity))
        {
          rebuild = true;
          break;
        }
      }

      if (rebuild)
      {
        break;
      }
    }
  }

  if (rebuild)
  {
    for (u32 i = 0; i < internal->count; ++i)
    {
      if (!s_grouping_rebuild(ecs, internal->groupings[i]))
      {
        internal->rebuild_pending = true;
        return false;
      }
    }
  }

  for (u32 i = 0; i < internal->dirty_count; ++i)
  {
    x_hashtable_grouping_slot_remove(
        internal->dirty_set, s_entity_key(internal->dirty_entities[i]));
  }
  internal->dirty_count = 0;
  internal->rebuild_pending = false;
  return true;
}

static bool s_grouping_callback_sync(void *user)
{
  return s_grouping_flush((LDKECS *)user);
}

static void s_grouping_systems_unbind_group(
    LDKECS *ecs, const LDKEntityGroup *group)
{
  u32 count;

  if (!ecs || !group)
  {
    return;
  }

  count = ldk_system_registry_count(&ecs->system);
  for (u32 i = 0; i < count; ++i)
  {
    LDKSystemDesc desc = {0};
    if (ldk_system_registry_at(&ecs->system, i, &desc) &&
        ldk_system_registry_system_group_get(&ecs->system, desc.id) == group)
    {
      ldk_system_registry_system_group_set(&ecs->system, desc.id, NULL);
    }
  }
}

static bool s_parse_u64(const char *text, u64 *out)
{
  unsigned long long value;
  char *end;

  if (!text || !*text || !out || text[0] == '-')
  {
    return false;
  }

  errno = 0;
  value = strtoull(text, &end, 0);
  if (errno == ERANGE || end == text || *end != 0 || value == 0)
  {
    return false;
  }

  *out = (u64)value;
  return true;
}

static bool s_parse_u32(const char *text, u32 *out, bool allow_zero)
{
  unsigned long value;
  char *end;

  if (!text || !*text || !out || text[0] == '-')
  {
    return false;
  }

  errno = 0;
  value = strtoul(text, &end, 0);
  if (errno == ERANGE || end == text || *end != 0 || value > UINT32_MAX ||
      (!allow_zero && value == 0))
  {
    return false;
  }

  *out = (u32)value;
  return true;
}

static void s_grouping_config_entries_free(
    LDKGroupingConfigEntry *entries, u32 count)
{
  if (!entries)
  {
    return;
  }

  for (u32 i = 0; i < count; ++i)
  {
    free(entries[i].component_types);
    free(entries[i].name);
  }
  free(entries);
}

static bool s_grouping_config_entries_load(
    XIni *ini, LDKGroupingConfigEntry **out_entries, u32 *out_count)
{
  LDKGroupingConfigEntry *entries = NULL;
  const char *count_text;
  int section = -1;
  u32 count = 0;

  if (!ini || !out_entries || !out_count)
  {
    return false;
  }

  *out_entries = NULL;
  *out_count = 0;
  count_text = x_ini_get(ini, "groupings", "count", NULL);
  if (!count_text)
  {
    return true;
  }

  if (!s_parse_u32(count_text, &count, true))
  {
    return false;
  }

  for (int i = 0; i < x_ini_section_count(ini); ++i)
  {
    if (strcmp(x_ini_section_name(ini, i), "groupings") == 0)
    {
      section = i;
      break;
    }
  }

  if (section < 0 ||
      count > (u32)x_ini_key_count(ini, section))
  {
    return false;
  }

  if (count)
  {
    entries = (LDKGroupingConfigEntry *)calloc(count, sizeof(*entries));
    if (!entries)
    {
      return false;
    }
  }

  for (u32 i = 0; i < count; ++i)
  {
    char key[64];
    const char *value;
    u32 component_count;

    snprintf(key, sizeof(key), "%u.id", i);
    value = x_ini_get(ini, "groupings", key, NULL);
    if (!s_parse_u64(value, &entries[i].id))
    {
      goto failed;
    }

    snprintf(key, sizeof(key), "%u.name", i);
    value = x_ini_get(ini, "groupings", key, NULL);
    if (!value || !*value)
    {
      goto failed;
    }
    entries[i].name = s_string_copy(value);
    if (!entries[i].name)
    {
      goto failed;
    }

    snprintf(key, sizeof(key), "%u.component_count", i);
    value = x_ini_get(ini, "groupings", key, NULL);
    if (!s_parse_u32(value, &component_count, true) ||
        component_count > LDK_ENTITY_MAX_COMPONENTS)
    {
      goto failed;
    }
    entries[i].component_count = component_count;

    if (component_count)
    {
      entries[i].component_types =
          (u32 *)malloc(sizeof(u32) * (size_t)component_count);
      if (!entries[i].component_types)
      {
        goto failed;
      }
    }

    for (u32 component_index = 0; component_index < component_count;
         ++component_index)
    {
      snprintf(key, sizeof(key), "%u.component_%u", i, component_index);
      value = x_ini_get(ini, "groupings", key, NULL);
      if (!s_parse_u32(
              value, &entries[i].component_types[component_index], false))
      {
        goto failed;
      }
    }
  }

  *out_entries = entries;
  *out_count = count;
  return true;

failed:
  s_grouping_config_entries_free(entries, count);
  return false;
}

typedef struct LDKECSEntityForeachContext
{
  bool (*fn)(LDKEntity entity, void *user);
  void *user;
} LDKECSEntityForeachContext;

typedef struct LDKECSComponentTypeAtContext
{
  LDKEntity entity;
  u32 component_index;
  u32 *out_component_type;
  bool found;
} LDKECSComponentTypeAtContext;

static bool s_ecs_entity_foreach_adapter(
    LDKEntity entity, LDKEntityInfo *info, void *user)
{
  LDKECSEntityForeachContext *context =
      (LDKECSEntityForeachContext *)user;

  (void)info;

  if (!context || !context->fn)
  {
    return false;
  }

  return context->fn(entity, context->user);
}

static bool s_ecs_entity_component_type_at_adapter(
    LDKEntity entity, LDKEntityInfo *info, void *user)
{
  LDKECSComponentTypeAtContext *context =
      (LDKECSComponentTypeAtContext *)user;

  if (!context || !info)
  {
    return false;
  }

  if (entity.index != context->entity.index ||
      entity.version != context->entity.version)
  {
    return true;
  }

  if (context->component_index >= info->components.component_count)
  {
    return false;
  }

  *context->out_component_type =
      info->components.component_type[context->component_index];
  context->found = true;
  return false;
}

// ---------------------------------------------------------------------------
// ECS lifecycle
// ---------------------------------------------------------------------------

bool ldk_ecs_initialize(
    LDKECS *context, u32 entity_page_capacity, u32 entity_initial_pages)
{
  LDKEntityRegistry *entity_registry;
  LDKComponentRegistry *component_registry;
  LDKSystemRegistry *system_registry;
  bool error = false;

  if (!context)
  {
    return false;
  }

  entity_registry = &context->entity;
  component_registry = &context->component;
  system_registry = &context->system;
  context->grouping_internal = NULL;

  if (!ldk_entity_module_initialize(
          entity_registry, entity_page_capacity, entity_initial_pages))
  {
    return false;
  }

  if (!ldk_component_registry_initialize(component_registry))
  {
    ldk_entity_module_terminate(entity_registry);
    return false;
  }

  if (!ldk_system_registry_initialize(system_registry))
  {
    ldk_component_registry_terminate(component_registry);
    ldk_entity_module_terminate(entity_registry);
    return false;
  }

  if (!s_grouping_registry_initialize(context))
  {
    ldk_system_registry_terminate(system_registry);
    ldk_component_registry_terminate(component_registry);
    ldk_entity_module_terminate(entity_registry);
    return false;
  }

  if (!ldk_system_registry_callback_sync_set(
          system_registry, s_grouping_callback_sync, context))
  {
    s_grouping_registry_terminate(context);
    ldk_system_registry_terminate(system_registry);
    ldk_component_registry_terminate(component_registry);
    ldk_entity_module_terminate(entity_registry);
    return false;
  }

  // Register internal components
  LDKComponentDesc transform_component_desc =
      ldk_transform_component_desc(LDK_DEFAULT_TRANSFORM_COUNT);
  if (!ldk_component_register(&context->component, &transform_component_desc))
  {
    ldk_log_error("Failed to register component: Transform.");
    error = true;
  }

  LDKComponentDesc camera_component_desc =
      ldk_camera_component_desc(LDK_DEFAULT_CAMERA_COUNT);
  if (!ldk_component_register(&context->component, &camera_component_desc))
  {
    ldk_log_error("Failed to register component: Camera.");
    error = true;
  }

  LDKComponentDesc meshsource_component_desc =
      ldk_mesh_source_component_desc(LDK_DEFAULT_MESHSOURCE_COUNT);
  if (!ldk_component_register(&context->component, &meshsource_component_desc))
  {
    ldk_log_error("Failed to register component: MeshSource.");
    error = true;
  }

  if (error)
  {
    s_grouping_registry_terminate(context);
    ldk_component_registry_terminate(&context->component);
    ldk_entity_module_terminate(&context->entity);
    ldk_system_registry_terminate(&context->system);
    return false;
  }

  return true;
}

void ldk_ecs_terminate(void)
{
  LDKECS *ecs = s_ecs();
  LDKEntityRegistry *entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry *component_registry = ldk_ecs_component_registry_get();
  LDKSystemRegistry *system_registry = ldk_ecs_system_registry_get();

  if (system_registry)
  {
    ldk_system_registry_terminate(system_registry);
  }

  if (ecs)
  {
    s_grouping_registry_terminate(ecs);
  }

  if (component_registry)
  {
    /* Registry storage teardown does not invoke component callbacks. Release
     * mesh-owned renderer resources while their owners and renderer live.
     * Do not detach here: iteration must not swap/remove store entries.
     */
    XArray *owners = ldk_component_owners_get(
        component_registry, LDK_COMPONENT_TYPE_MESH_SOURCE);
    if (owners && entity_registry)
    {
      for (u32 i = 0; i < x_array_count(owners); i++)
      {
        LDKEntity *owner = x_array_get(owners, i);
        ldk_component_destroy_data(component_registry, entity_registry, *owner,
            LDK_COMPONENT_TYPE_MESH_SOURCE, i);
      }
    }
    ldk_component_registry_terminate(component_registry);
  }

  if (entity_registry)
  {
    ldk_entity_module_terminate(entity_registry);
  }
}

// ---------------------------------------------------------------------------
// Entity lifecycle
// ---------------------------------------------------------------------------

LDKEntity ldk_ecs_entity_create(void)
{
  LDKECS *ecs = s_ecs();
  LDKEntityRegistry *entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry *component_registry = ldk_ecs_component_registry_get();

  if (!ecs || !entity_registry || !component_registry)
  {
    return x_handle_null();
  }

  LDKEntity entity = ldk_entity_create(entity_registry);

  if (x_handle_is_null(entity))
  {
    return entity;
  }

  // Entities always have a transform component
  if (!ldk_entity_component_add(entity_registry, component_registry, entity,
          LDK_COMPONENT_TYPE_TRANSFORM, NULL))
  {
    ldk_entity_destroy(entity_registry, entity);
    return x_handle_null();
  }

  s_grouping_entity_dirty(ecs, entity);
  return entity;
}

void ldk_ecs_entity_destroy(LDKEntity entity)
{
  LDKECS *ecs = s_ecs();
  LDKEntityRegistry *entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry *component_registry = ldk_ecs_component_registry_get();

  if (!ecs || !entity_registry || !component_registry)
  {
    return;
  }

  if (!ldk_entity_is_alive(entity_registry, entity))
  {
    return;
  }

  ldk_component_registry_remove_all(component_registry, entity_registry, entity);
  ldk_entity_destroy(entity_registry, entity);
  s_grouping_entity_dirty(ecs, entity);
}

// ---------------------------------------------------------------------------
// Component management
// ---------------------------------------------------------------------------

void *ldk_ecs_component_add(
    LDKEntity entity, u32 component_type, const void *initial_value)
{
  LDKECS *ecs = s_ecs();
  LDKEntityRegistry *entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry *component_registry = ldk_ecs_component_registry_get();
  void *component;

  // We disallow attaching TRANSFORMS via this facade since it always attaches a
  // transform when creating an entity in ldk_ecs_entity_create().
  if (component_type == LDK_COMPONENT_TYPE_TRANSFORM)
  {
    return NULL;
  }

  if (!ecs || !entity_registry || !component_registry)
  {
    return NULL;
  }

  component = ldk_entity_component_add(entity_registry, component_registry,
      entity, component_type, initial_value);
  if (component)
  {
    s_grouping_entity_dirty(ecs, entity);
  }
  return component;
}

void *ldk_ecs_component_get(LDKEntity entity, u32 component_type)
{
  LDKEntityRegistry *entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry *component_registry = ldk_ecs_component_registry_get();

  if (!entity_registry || !component_registry)
  {
    return NULL;
  }

  return ldk_entity_component_get(
      entity_registry, component_registry, entity, component_type);
}

const void *ldk_ecs_component_get_const(LDKEntity entity, u32 component_type)
{
  return (const void *)ldk_ecs_component_get(entity, component_type);
}

bool ldk_ecs_entity_foreach(
    bool (*fn)(LDKEntity entity, void *user), void *user)
{
  LDKEntityRegistry *entity_registry = ldk_ecs_entity_registry_get();
  LDKECSEntityForeachContext context;

  if (!entity_registry || !fn)
  {
    return false;
  }

  context.fn = fn;
  context.user = user;

  ldk_entity_foreach(entity_registry, s_ecs_entity_foreach_adapter, &context);
  return true;
}

u32 ldk_ecs_entity_component_count(LDKEntity entity)
{
  LDKEntityRegistry *entity_registry = ldk_ecs_entity_registry_get();

  if (!entity_registry)
  {
    return 0;
  }

  return ldk_entity_component_count(entity_registry, entity);
}

bool ldk_ecs_entity_component_type_at(
    LDKEntity entity, u32 component_index, u32 *out_component_type)
{
  LDKEntityRegistry *entity_registry = ldk_ecs_entity_registry_get();
  LDKECSComponentTypeAtContext context;

  if (out_component_type)
  {
    *out_component_type = 0;
  }

  if (!entity_registry || !out_component_type)
  {
    return false;
  }

  context.entity = entity;
  context.component_index = component_index;
  context.out_component_type = out_component_type;
  context.found = false;

  ldk_entity_foreach(
      entity_registry, s_ecs_entity_component_type_at_adapter, &context);
  return context.found;
}

const char *ldk_ecs_entity_name_get(LDKEntity entity)
{
  LDKEntityRegistry *entity_registry = ldk_ecs_entity_registry_get();

  if (!entity_registry)
  {
    return NULL;
  }

  return ldk_entity_name_get(entity_registry, entity);
}

bool ldk_ecs_entity_name_set(LDKEntity entity, const char *name)
{
  LDKEntityRegistry *entity_registry = ldk_ecs_entity_registry_get();

  if (!entity_registry)
  {
    return false;
  }

  return ldk_entity_name_set(entity_registry, entity, name);
}

bool ldk_ecs_component_remove(LDKEntity entity, u32 component_type)
{
  LDKECS *ecs = s_ecs();
  LDKEntityRegistry *entity_registry = ldk_ecs_entity_registry_get();
  LDKComponentRegistry *component_registry = ldk_ecs_component_registry_get();
  bool removed;

  // Transform components can not be removed
  if (component_type == LDK_COMPONENT_TYPE_TRANSFORM)
  {
    return false;
  }

  if (!ecs || !entity_registry || !component_registry)
  {
    return false;
  }

  removed = ldk_entity_component_remove(
      entity_registry, component_registry, entity, component_type);
  if (removed)
  {
    s_grouping_entity_dirty(ecs, entity);
  }
  return removed;
}

bool ldk_ecs_component_register(const LDKComponentDesc *desc)
{
  LDKComponentRegistry *component_registry = ldk_ecs_component_registry_get();

  if (!component_registry)
  {
    return false;
  }
  return ldk_component_register(component_registry, desc);
}

// ---------------------------------------------------------------------------
// Grouping management
// ---------------------------------------------------------------------------

bool ldk_ecs_grouping_register(const LDKGroupingDesc *desc)
{
  LDKECS *ecs = s_ecs();
  LDKGroupingRegistryInternal *internal = s_grouping_internal(ecs);
  LDKRegisteredGrouping *grouping;

  if (!ecs || !internal || ldk_game_instance_is_started() ||
      ldk_system_registry_is_busy(&ecs->system) ||
      !s_grouping_desc_is_valid(ecs, desc, false) ||
      !s_grouping_registry_reserve(internal, internal->count + 1u))
  {
    return false;
  }

  grouping = s_grouping_create(ecs, desc, false);
  if (!grouping)
  {
    return false;
  }

  internal->groupings[internal->count++] = grouping;
  return true;
}

bool ldk_ecs_grouping_unregister(u64 id)
{
  LDKECS *ecs = s_ecs();
  LDKGroupingRegistryInternal *internal = s_grouping_internal(ecs);

  if (!ecs || !internal || id == 0 || ldk_game_instance_is_started() ||
      ldk_system_registry_is_busy(&ecs->system))
  {
    return false;
  }

  for (u32 i = 0; i < internal->count; ++i)
  {
    if (internal->groupings[i]->desc.id == id)
    {
      s_grouping_systems_unbind_group(ecs, &internal->groupings[i]->view);
      s_grouping_destroy(internal->groupings[i]);
      memmove(&internal->groupings[i], &internal->groupings[i + 1u],
          sizeof(*internal->groupings) *
              (size_t)(internal->count - i - 1u));
      internal->count -= 1u;
      return true;
    }
  }

  return false;
}

bool ldk_ecs_grouping_find_by_id(u64 id, LDKGroupingDesc *out)
{
  LDKECS *ecs = s_ecs();
  const LDKGroupingRegistryInternal *internal = s_grouping_internal(ecs);
  const LDKRegisteredGrouping *grouping;

  if (!out)
  {
    return false;
  }

  grouping = s_grouping_find_const(internal, id);
  if (!grouping)
  {
    return false;
  }

  *out = grouping->desc;
  return true;
}

u32 ldk_ecs_grouping_count(void)
{
  LDKECS *ecs = s_ecs();
  const LDKGroupingRegistryInternal *internal = s_grouping_internal(ecs);
  return internal ? internal->count : 0;
}

bool ldk_ecs_grouping_at(u32 index, LDKGroupingDesc *out)
{
  LDKECS *ecs = s_ecs();
  const LDKGroupingRegistryInternal *internal = s_grouping_internal(ecs);

  if (!internal || !out || index >= internal->count)
  {
    return false;
  }

  *out = internal->groupings[index]->desc;
  return true;
}

const LDKEntityGroup *ldk_ecs_grouping_get(u64 id)
{
  LDKECS *ecs = s_ecs();
  LDKGroupingRegistryInternal *internal = s_grouping_internal(ecs);
  LDKRegisteredGrouping *grouping;

  if (!ecs || !internal || id == 0)
  {
    return NULL;
  }

  if (!ldk_system_registry_is_busy(&ecs->system) && !s_grouping_flush(ecs))
  {
    return NULL;
  }

  grouping = s_grouping_find(internal, id);
  return grouping ? &grouping->view : NULL;
}

bool ldk_ecs_grouping_is_config_defined(u64 id)
{
  LDKECS *ecs = s_ecs();
  const LDKGroupingRegistryInternal *internal = s_grouping_internal(ecs);
  const LDKRegisteredGrouping *grouping = s_grouping_find_const(internal, id);
  return grouping && grouping->config_defined;
}

bool ldk_ecs_grouping_configure_file(const char *ini_path)
{
  LDKECS *ecs = s_ecs();
  LDKGroupingRegistryInternal *internal = s_grouping_internal(ecs);
  LDKGroupingConfigEntry *entries = NULL;
  LDKRegisteredGrouping **new_groupings = NULL;
  XIni ini = {0};
  XIniError error = {0};
  u32 entry_count = 0;
  u32 code_count = 0;
  bool ok = false;

  if (!ecs || !internal || !ini_path || ldk_game_instance_is_started() ||
      ldk_system_registry_is_busy(&ecs->system))
  {
    return false;
  }

  if (!x_ini_load_file(ini_path, &ini, &error))
  {
    return false;
  }

  if (!s_grouping_config_entries_load(&ini, &entries, &entry_count))
  {
    goto done;
  }

  for (u32 i = 0; i < entry_count; ++i)
  {
    LDKGroupingDesc desc = {.id = entries[i].id,
        .name = entries[i].name,
        .component_types = entries[i].component_types,
        .component_count = entries[i].component_count};

    if (!s_grouping_desc_is_valid(ecs, &desc, true))
    {
      goto done;
    }

    for (u32 j = 0; j < i; ++j)
    {
      if (entries[j].id == entries[i].id ||
          strcmp(entries[j].name, entries[i].name) == 0)
      {
        goto done;
      }
    }
  }

  if (entry_count)
  {
    new_groupings = (LDKRegisteredGrouping **)calloc(
        entry_count, sizeof(*new_groupings));
    if (!new_groupings)
    {
      goto done;
    }
  }

  for (u32 i = 0; i < entry_count; ++i)
  {
    LDKGroupingDesc desc = {.id = entries[i].id,
        .name = entries[i].name,
        .component_types = entries[i].component_types,
        .component_count = entries[i].component_count};
    new_groupings[i] = s_grouping_create(ecs, &desc, true);
    if (!new_groupings[i])
    {
      goto done;
    }
  }

  for (u32 i = 0; i < internal->count; ++i)
  {
    if (!internal->groupings[i]->config_defined)
    {
      code_count += 1u;
    }
  }

  if (!s_grouping_registry_reserve(internal, code_count + entry_count))
  {
    goto done;
  }

  u32 write_index = 0;
  for (u32 i = 0; i < internal->count; ++i)
  {
    LDKRegisteredGrouping *grouping = internal->groupings[i];
    if (grouping->config_defined)
    {
      s_grouping_systems_unbind_group(ecs, &grouping->view);
      s_grouping_destroy(grouping);
    }
    else
    {
      internal->groupings[write_index++] = grouping;
    }
  }

  for (u32 i = 0; i < entry_count; ++i)
  {
    internal->groupings[write_index++] = new_groupings[i];
    new_groupings[i] = NULL;
  }
  internal->count = write_index;
  ok = true;

done:
  if (new_groupings)
  {
    for (u32 i = 0; i < entry_count; ++i)
    {
      s_grouping_destroy(new_groupings[i]);
    }
  }
  free(new_groupings);
  s_grouping_config_entries_free(entries, entry_count);
  x_ini_free(&ini);
  return ok;
}

// ---------------------------------------------------------------------------
// System management
// ---------------------------------------------------------------------------

bool ldk_ecs_system_register(const LDKSystemDesc *desc)
{
  LDKSystemRegistry *system_registry = ldk_ecs_system_registry_get();

  if (!system_registry)
  {
    return false;
  }

  return ldk_system_registry_register(system_registry, desc);
}

bool ldk_ecs_system_unregister(u64 id)
{
  LDKSystemRegistry *system_registry = ldk_ecs_system_registry_get();

  if (!system_registry)
  {
    return false;
  }

  return ldk_system_registry_unregister(system_registry, id);
}

bool ldk_ecs_system_start(u64 id)
{
  LDKSystemRegistry *registry = ldk_ecs_system_registry_get();
  return registry && ldk_system_registry_system_start(registry, id);
}

bool ldk_ecs_system_stop(u64 id)
{
  LDKSystemRegistry *registry = ldk_ecs_system_registry_get();
  return registry && ldk_system_registry_system_stop(registry, id);
}

bool ldk_ecs_system_is_started(u64 id)
{
  LDKSystemRegistry *registry = ldk_ecs_system_registry_get();
  return registry && ldk_system_registry_system_is_started(registry, id);
}

bool ldk_ecs_system_pause(void)
{
  LDKSystemRegistry *registry = ldk_ecs_system_registry_get();
  return registry && ldk_system_registry_pause(registry);
}

bool ldk_ecs_system_resume(void)
{
  LDKSystemRegistry *registry = ldk_ecs_system_registry_get();
  return registry && ldk_system_registry_resume(registry);
}

bool ldk_ecs_system_is_paused(void)
{
  LDKSystemRegistry *registry = ldk_ecs_system_registry_get();
  return registry && ldk_system_registry_is_paused(registry);
}

// ---------------------------------------------------------------------------
//  Engine internal utility
// ---------------------------------------------------------------------------

#ifdef LDK_ENGINE

LDKEntityRegistry *ldk_ecs_entity_registry_get(void)
{
  LDKECS *ecs = (LDKECS *)ldk_module_get(LDK_MODULE_ECS);
  return ecs ? &ecs->entity : NULL;
}

LDKComponentRegistry *ldk_ecs_component_registry_get(void)
{
  LDKECS *ecs = (LDKECS *)ldk_module_get(LDK_MODULE_ECS);
  return ecs ? &ecs->component : NULL;
}

LDKSystemRegistry *ldk_ecs_system_registry_get(void)
{
  LDKECS *ecs = (LDKECS *)ldk_module_get(LDK_MODULE_ECS);
  return ecs ? &ecs->system : NULL;
}

bool ldk_ecs_system_registry_start(LDKECS *context)
{
  return context && ldk_system_registry_start(&context->system);
}

bool ldk_ecs_system_registry_stop(LDKECS *context)
{
  return context && ldk_system_registry_stop(&context->system);
}

bool ldk_ecs_system_bucket_run(
    LDKECS *context, LDKSystemBucket bucket, float dt)
{
  bool result;

  if (!context || !context->system.is_started)
  {
    return false;
  }

  if (!s_grouping_flush(context))
  {
    return false;
  }

  result = ldk_system_registry_run_bucket(&context->system, bucket, dt);

  if (!s_grouping_flush(context))
  {
    return false;
  }

  return result;
}

#endif
