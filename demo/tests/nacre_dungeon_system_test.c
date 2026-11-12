/* Standalone ECS lifecycle integration test with real LDK public headers.
 * The only mocked APIs are the LDK runtime/asset manager, so we can test
 * prefab parenting, child-tree cleanup and transactional regeneration.
 */
#include "../src/system/nacre_dungeon_system.h"

#include <component/ldk_transform.h>
#include <ldk.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define NACRE_TEST_MAX_ENTITIES 50000u

typedef struct NacreTestEntity
{
  bool alive;
  LDKTransform transform;
} NacreTestEntity;

static NacreTestEntity nacre_test_entities[NACRE_TEST_MAX_ENTITIES];
static u32 nacre_test_next_id;
static u32 nacre_test_alive;
static u32 nacre_test_spawn_calls;
static u32 nacre_test_fail_on_spawn;
static int nacre_test_asset_manager;

/* Small STDX primitives ordinarily linked from STDX itself. */
XHandle x_handle_null(void)
{
  XHandle result = {.index = X_HPOOL_NULL_INDEX, .version = 0u};
  return result;
}
int x_handle_is_null(XHandle handle)
{
  return handle.index == X_HPOOL_NULL_INDEX;
}
Vec3 vec3_make(float x, float y, float z)
{
  Vec3 result = {x, y, z};
  return result;
}

static LDKEntity nacre_test_entity_create(void)
{
  assert(nacre_test_next_id < NACRE_TEST_MAX_ENTITIES);
  LDKEntity out = {.index = nacre_test_next_id++, .version = 1u};
  NacreTestEntity *entity = &nacre_test_entities[out.index];
  assert(!entity->alive);
  entity->alive = true;
  entity->transform.parent = x_handle_null();
  entity->transform.first_child = x_handle_null();
  entity->transform.prev_sibling = x_handle_null();
  entity->transform.next_sibling = x_handle_null();
  ++nacre_test_alive;
  return out;
}
static NacreTestEntity *nacre_test_find(LDKEntity e)
{
  if (x_handle_is_null(e) || e.version != 1u ||
      e.index >= nacre_test_next_id || !nacre_test_entities[e.index].alive)
  {
    return NULL;
  }
  return &nacre_test_entities[e.index];
}

/* Required subset of actual LDK ECS / asset interfaces. */
void *ldk_module_get(LDKModuleType kind)
{
  return kind == LDK_MODULE_ASSET_MANAGER ? &nacre_test_asset_manager : NULL;
}
LDKAssetPrefab ldk_asset_prefab_null(void)
{
  LDKAssetPrefab prefab = {.h = x_handle_null()};
  return prefab;
}
bool ldk_asset_manager_prefab_is_alive(LDKAssetManager *manager, LDKAssetPrefab prefab)
{
  return manager != NULL && !x_handle_is_null(prefab.h) && prefab.h.version == 1u;
}
LDKEntity ldk_ecs_entity_create(void) { return nacre_test_entity_create(); }
bool ldk_ecs_entity_name_set(LDKEntity e, const char *name)
{
  return nacre_test_find(e) != NULL && name != NULL;
}

bool ldk_transform_set_local_position(LDKEntity e, Vec3 pos)
{
  NacreTestEntity *entity = nacre_test_find(e);
  if (!entity) return false;
  entity->transform.local_position = pos;
  return true;
}
LDKEntity ldk_transform_get_parent(LDKEntity e)
{
  const NacreTestEntity *entity = nacre_test_find(e);
  return entity ? entity->transform.parent : x_handle_null();
}
bool ldk_transform_set_parent(LDKEntity child, LDKEntity parent)
{
  NacreTestEntity *c = nacre_test_find(child);
  NacreTestEntity *p = nacre_test_find(parent);
  if (!c || !p || !x_handle_is_null(c->transform.parent)) return false;
  c->transform.parent = parent;
  c->transform.next_sibling = p->transform.first_child;
  if (!x_handle_is_null(p->transform.first_child))
    nacre_test_find(p->transform.first_child)->transform.prev_sibling = child;
  p->transform.first_child = child;
  return true;
}
const void *ldk_ecs_component_get_const(LDKEntity e, u32 type)
{
  NacreTestEntity *entity = nacre_test_find(e);
  return entity && type == LDK_COMPONENT_TYPE_TRANSFORM ? &entity->transform : NULL;
}
LDKEntity ldk_ecs_prefab_instantiate(LDKAssetPrefab prefab)
{
  assert(!x_handle_is_null(prefab.h));
  if (++nacre_test_spawn_calls == nacre_test_fail_on_spawn)
    return x_handle_null();
  LDKEntity root = nacre_test_entity_create();
  /* Every prefab has one nested child: deleting only its root leaks it. */
  LDKEntity child = nacre_test_entity_create();
  assert(ldk_transform_set_parent(child, root));
  return root;
}
void ldk_ecs_entity_destroy(LDKEntity e)
{
  NacreTestEntity *entity = nacre_test_find(e);
  if (!entity) return;
  /* Matches the LDK: destruction ORPHANS (doesn't delete) children. */
  LDKEntity parent = entity->transform.parent;
  if (!x_handle_is_null(parent))
  {
    NacreTestEntity *p = nacre_test_find(parent);
    if (p && p->transform.first_child.index == e.index)
      p->transform.first_child = entity->transform.next_sibling;
    if (!x_handle_is_null(entity->transform.prev_sibling))
      nacre_test_find(entity->transform.prev_sibling)->transform.next_sibling = entity->transform.next_sibling;
    if (!x_handle_is_null(entity->transform.next_sibling))
      nacre_test_find(entity->transform.next_sibling)->transform.prev_sibling = entity->transform.prev_sibling;
  }
  for (LDKEntity c = entity->transform.first_child; !x_handle_is_null(c);)
  {
    NacreTestEntity *child = nacre_test_find(c);
    LDKEntity next = child->transform.next_sibling;
    child->transform.parent = x_handle_null();
    child->transform.next_sibling = x_handle_null();
    child->transform.prev_sibling = x_handle_null();
    c = next;
  }
  entity->alive = false;
  --nacre_test_alive;
}

void x_log_message(XLogger *logger, XLogLevel level, XLogColor fg,
    XLogColor bg, XLogComponent components, const char *file, int line,
    const char *func, const char *fmt, ...)
{
  (void)logger; (void)level; (void)fg; (void)bg; (void)components;
  (void)file; (void)line; (void)func;
  (void)fmt;
}
size_t x_smallstr_from_cstr(XSmallstr *s, const char *value)
{
  size_t n = strlen(value);
  if (n > X_SMALLSTR_MAX_LENGTH) n = X_SMALLSTR_MAX_LENGTH;
  memcpy(s->buf, value, n);
  s->buf[n] = '\0';
  s->length = n;
  return n;
}
size_t x_smallstr_length(const XSmallstr *s) { return s->length; }
const char *x_smallstr_cstr(const XSmallstr *s) { return s->buf; }

static LDKAssetPrefab nacre_test_prefab(u32 index)
{
  LDKAssetPrefab p = {.h = {.index = index, .version = 1u}};
  return p;
}

int main(void)
{
  NacreDungeonSystem system = {0};
  /* Assigning a prefab before the first Play MUST survive default values. */
  system.floor_prefab = nacre_test_prefab(100u);
  assert(nacre_dungeon_system_initialize(&system) == 0);
  assert(system.floor_prefab.h.index == 100u);
  assert(system.generated_entity_count > 0u);
  assert(system.layout_generated && system.layout_valid && system.room_count == 19u);
  assert(system.planned_piece_count > 0u);
  assert(nacre_test_alive == 1u + system.generated_entity_count * 2u);
  assert(nacre_dungeon_system_map_get() != NULL);

  system.floor_prefab = nacre_test_prefab(101u);
  system.wall_x_1m_prefab = nacre_test_prefab(102u);
  system.wall_z_1m_prefab = nacre_test_prefab(103u);
  system.wall_x_2m_prefab = nacre_test_prefab(104u);
  system.wall_z_2m_prefab = nacre_test_prefab(105u);
  system.passage_x_prefab = nacre_test_prefab(106u);
  system.passage_z_prefab = nacre_test_prefab(107u);
  system.hole_x_prefab = nacre_test_prefab(108u);
  system.hole_z_prefab = nacre_test_prefab(109u);
  system.cracked_x_prefab = nacre_test_prefab(110u);
  system.cracked_z_prefab = nacre_test_prefab(111u);
  system.world_origin = vec3_make(8.0f, 0.0f, -3.0f);
  system.regenerate = true;
  nacre_dungeon_system_update(&system, NULL, .016f);
  assert(system.generation_revision == 2u);
  assert(system.generated_entity_count > 500u);
  assert(system.missing_prefab_pieces == 0u);
  assert(system.planned_piece_count == system.generated_entity_count);
  assert(nacre_test_alive == 1u + system.generated_entity_count * 2u);
  u32 stable_count = nacre_test_alive;
  u32 stable_revision = system.generation_revision;

  /* Failed instantiate halfway must roll back the new tree and retain old. */
  nacre_test_fail_on_spawn = nacre_test_spawn_calls + 21u;
  system.regenerate = true;
  nacre_dungeon_system_update(&system, NULL, .016f);
  assert(!system.regenerate);
  assert(system.generation_revision == stable_revision);
  assert(nacre_test_alive == stable_count);
  nacre_test_fail_on_spawn = 0u;

  system.regenerate = true;
  nacre_dungeon_system_update(&system, NULL, .016f);
  assert(system.generation_revision == stable_revision + 1u);
  assert(nacre_test_alive == stable_count);

  /* Failure in C generator also preserves already instantiated objects. */
  system.width = 1u;
  system.regenerate = true;
  nacre_dungeon_system_update(&system, NULL, .016f);
  assert(system.last_result == NACRE_DUNGEON_INVALID_CONFIG);
  assert(nacre_test_alive == stable_count);
  system.width = 50u;

  /* Unassign every prefab: existing entities must be removed on reroll. */
  system.floor_prefab = ldk_asset_prefab_null();
  system.wall_x_1m_prefab = ldk_asset_prefab_null();
  system.wall_z_1m_prefab = ldk_asset_prefab_null();
  system.wall_x_2m_prefab = ldk_asset_prefab_null();
  system.wall_z_2m_prefab = ldk_asset_prefab_null();
  system.passage_x_prefab = ldk_asset_prefab_null();
  system.passage_z_prefab = ldk_asset_prefab_null();
  system.hole_x_prefab = ldk_asset_prefab_null();
  system.hole_z_prefab = ldk_asset_prefab_null();
  system.cracked_x_prefab = ldk_asset_prefab_null();
  system.cracked_z_prefab = ldk_asset_prefab_null();
  system.regenerate = true;
  nacre_dungeon_system_update(&system, NULL, .016f);
  assert(system.generated_entity_count == 0u);
  assert(system.missing_prefab_pieces == system.planned_piece_count);
  assert(nacre_test_alive == 0u);
  nacre_dungeon_system_terminate(&system);
  assert(!system.layout_generated && nacre_dungeon_system_map_get() == NULL);
  assert(nacre_test_alive == 0u);
  /* Regression: Comet may deserialize only SOME integer generation fields,
   * with the rest left zero. Defaults must fill each missing positive value
   * independently, retaining an explicitly authored seed and probabilities. */
  NacreDungeonConfig config;
  nacre_dungeon_generator_config_defaults(&config);
  strcpy(config.seed, "1");
  assert(nacre_dungeon_generator_config_error(&config) == NULL);
  config.height = 0u;
  assert(strcmp(nacre_dungeon_generator_config_error(&config),
      "Height must be 20..80") == 0);
  config.height = 50u;
  config.rooms_min = 23u;
  config.rooms_max = 19u;
  assert(strcmp(nacre_dungeon_generator_config_error(&config),
      "Rooms Min exceeds Rooms Max") == 0);

  NacreDungeonSystem partial = {0};
  x_smallstr_from_cstr(&partial.seed, "1");
  partial.width = 50u;
  partial.rooms_min = 18u;
  partial.extra_connections = 0.0f; /* Valid explicitly authored zero. */
  partial.generate_on_start = true;
  partial.floor_prefab = nacre_test_prefab(201u);
  assert(nacre_dungeon_system_initialize(&partial) == 0);
  assert(partial.last_result == NACRE_DUNGEON_OK);
  assert(partial.layout_generated && partial.layout_valid);
  assert(partial.width == 50u && partial.height == 50u);
  assert(partial.rooms_min == 18u && partial.rooms_max == 22u);
  assert(partial.max_room_size == 6u && partial.max_corridor_tiles == 5u);
  assert(partial.extra_connections == 0.0f);
  assert(strcmp(x_smallstr_cstr(&partial.seed), "1") == 0);
  assert(partial.generated_entity_count > 0u);
  nacre_dungeon_system_terminate(&partial);
  assert(nacre_test_alive == 0u);

  /* Nonzero but impossible authoring values are NOT silently corrected. */
  NacreDungeonSystem invalid = {0};
  x_smallstr_from_cstr(&invalid.seed, "1");
  invalid.width = 50u;
  invalid.height = 50u;
  invalid.rooms_min = 22u;
  invalid.rooms_max = 18u;
  invalid.max_room_size = 6u;
  invalid.max_corridor_tiles = 5u;
  invalid.generate_on_start = true;
  assert(nacre_dungeon_system_initialize(&invalid) == 0);
  assert(invalid.last_result == NACRE_DUNGEON_INVALID_CONFIG);
  assert(!invalid.layout_generated);
  assert(strcmp(x_smallstr_cstr(&invalid.status),
      "Rooms Min exceeds Rooms Max") == 0);
  nacre_dungeon_system_terminate(&invalid);
  assert(nacre_test_alive == 0u);

  printf("Nacre prefab system lifecycle, rollback and subtree cleanup OK\n");
  return 0;
}
