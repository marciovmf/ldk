/* Standalone ECS lifecycle integration test with real LDK public headers.
 * The only mocked APIs are the LDK runtime/asset manager, so we can test
 * prefab parenting, child-tree cleanup and transactional regeneration.
 */
#include "../src/system/nacre_dungeon_system.h"

#include <component/ldk_transform.h>
#include <component/ldk_keyframe_animation_source.h>
#include <ldk.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_ecs.h>

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define NACRE_TEST_MAX_ENTITIES 50000u

typedef struct NacreTestEntity
{
  bool alive;
  LDKTransform transform;
  LDKKeyFrameAnimationSource animation;
  LDKAssetKeyframeAnimation clips[2];
  bool has_animation;
  u32 spawned_prefab;
} NacreTestEntity;

static NacreTestEntity nacre_test_entities[NACRE_TEST_MAX_ENTITIES];
static u32 nacre_test_next_id;
static u32 nacre_test_alive;
static u32 nacre_test_spawn_calls;
static u32 nacre_test_colored_gate_spawns;
static u32 nacre_test_fail_on_spawn;
static int nacre_test_asset_manager;
static u32 nacre_test_animation_stops;
static u32 nacre_test_animation_plays;

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
Quat quat_axis_angle(Vec3 axis, float angle)
{
  const float s = sinf(angle * .5f);
  return (Quat){axis.x * s, axis.y * s, axis.z * s, cosf(angle * .5f)};
}
Quat quat_mul(Quat a, Quat b)
{
  return (Quat){
      a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
      a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
      a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
      a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z};
}

static LDKEntity nacre_test_entity_create(void)
{
  assert(nacre_test_next_id < NACRE_TEST_MAX_ENTITIES);
  LDKEntity out = {.index = nacre_test_next_id++, .version = 1u};
  NacreTestEntity *entity = &nacre_test_entities[out.index];
  assert(!entity->alive);
  entity->alive = true;
  entity->transform.local_rotation = (Quat){0, 0, 0, 1};
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
bool ldk_transform_get_local_rotation(LDKEntity e, Quat *out)
{
  NacreTestEntity *entity = nacre_test_find(e);
  if (!entity || !out) return false;
  *out = entity->transform.local_rotation;
  return true;
}
bool ldk_transform_set_local_rotation(LDKEntity e, Quat rotation)
{
  NacreTestEntity *entity = nacre_test_find(e);
  if (!entity) return false;
  entity->transform.local_rotation = rotation;
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
  if (!entity) return NULL;
  if (type == LDK_COMPONENT_TYPE_TRANSFORM) return &entity->transform;
  if (type == LDK_COMPONENT_TYPE_KEYFRAME_ANIMATION_SOURCE &&
      entity->has_animation) return &entity->animation;
  return NULL;
}
void *ldk_ecs_component_get(LDKEntity e, u32 type)
{
  return (void *)ldk_ecs_component_get_const(e, type);
}

bool ldk_asset_manager_keyframe_animation_is_alive(
    LDKAssetManager *manager, LDKAssetKeyframeAnimation animation)
{
  return manager != NULL && animation.h.version == 1u;
}

const LDKAssetInfo *ldk_asset_get_info_const(
    LDKAssetManager *manager, LDKAssetHandle handle)
{
  static LDKAssetInfo info;
  if (!manager) return NULL;
  memset(&info, 0, sizeof(info));
  const char *path = handle.h.index == 301u ?
      "runtree/anims/idle.anim" : handle.h.index == 302u ?
      "runtree/anims/open.anim" : NULL;
  if (!path) return NULL;
  info.asset_path.length = strlen(path);
  memcpy(info.asset_path.buf, path, info.asset_path.length + 1u);
  return &info;
}

bool ldk_keyframe_animation_source_set_current(LDKEntity root, i32 index)
{
  NacreTestEntity *entity = nacre_test_find(root);
  if (!entity || !entity->has_animation || index < 0 ||
      (u32)index >= entity->animation.animation_count) return false;
  entity->animation.current_animation = index;
  return true;
}
void ldk_keyframe_animation_source_stop(LDKEntity root)
{
  NacreTestEntity *entity = nacre_test_find(root);
  assert(entity && entity->has_animation);
  ++nacre_test_animation_stops;
}
bool ldk_keyframe_animation_source_play(LDKEntity root)
{
  NacreTestEntity *entity = nacre_test_find(root);
  if (!entity || !entity->has_animation ||
      entity->animation.current_animation != 1 ||
      entity->animation.loop || entity->animation.play_on_start) return false;
  ++nacre_test_animation_plays;
  return true;
}

bool ldk_ecs_component_is_enabled(LDKEntity entity, u32 component_type)
{
  (void)component_type;
  return nacre_test_find(entity) != NULL;
}
bool ldk_ecs_component_enabled_set(
    LDKEntity entity, u32 component_type, bool enabled)
{
  (void)component_type; (void)enabled;
  return nacre_test_find(entity) != NULL;
}
LDKEntity ldk_ecs_prefab_instantiate(LDKAssetPrefab prefab)
{
  assert(!x_handle_is_null(prefab.h));
  if (++nacre_test_spawn_calls == nacre_test_fail_on_spawn)
    return x_handle_null();
  if (prefab.h.index >= 123u && prefab.h.index <= 128u)
    ++nacre_test_colored_gate_spawns;
  LDKEntity root = nacre_test_entity_create();
  nacre_test_find(root)->spawned_prefab = prefab.h.index;
  /* Verify that a user-authored root rotation is composed, not discarded
   * by the 90-degree alignment of Z-facing instances. */
  if (prefab.h.index == 102u)
    nacre_test_find(root)->transform.local_rotation =
        quat_axis_angle(vec3_make(1.0f, 0.0f, 0.0f), .2f);
  /* Some gate/door prefabs intentionally store "open" at index 1. The
   * dungeon must find it by filename, never assume index zero. */
  if ((prefab.h.index >= 114u && prefab.h.index <= 117u) ||
      (prefab.h.index >= 123u && prefab.h.index <= 128u))
  {
    NacreTestEntity *entity = nacre_test_find(root);
    entity->has_animation = true;
    entity->clips[0] = (LDKAssetKeyframeAnimation){.h = {.index = 301u, .version = 1u}};
    entity->clips[1] = (LDKAssetKeyframeAnimation){.h = {.index = 302u, .version = 1u}};
    entity->animation.animations = entity->clips;
    entity->animation.animation_count = 2u;
    entity->animation.current_animation = 0;
    entity->animation.play_on_start = true;
    entity->animation.loop = true;
  }
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
  assert(system.layout_generated && system.layout_valid && system.room_count >= 18u && system.room_count <= 22u);
  assert(system.planned_piece_count > 0u);
  assert(nacre_test_alive == 1u + system.generated_entity_count * 2u);
  assert(nacre_dungeon_system_map_get() != NULL);

  system.floor_prefab = nacre_test_prefab(101u);
  system.wall_1m_prefab = nacre_test_prefab(102u);
  system.wall_2m_prefab = nacre_test_prefab(104u);
  system.passage_prefab = nacre_test_prefab(106u);
  system.hole_prefab = nacre_test_prefab(108u);
  system.cracked_prefab = nacre_test_prefab(110u);
  system.pillar_intact_prefab = nacre_test_prefab(112u);
  system.pillar_broken_prefab = nacre_test_prefab(113u);
  system.door_prefab = nacre_test_prefab(114u);
  system.portcullis_prefab = nacre_test_prefab(116u);
  system.totem_prefab = nacre_test_prefab(118u);
  system.entry_prefab = nacre_test_prefab(121u);
  system.exit_prefab = nacre_test_prefab(122u);
  system.portcullis_red_prefab = nacre_test_prefab(123u);
  system.portcullis_green_prefab = nacre_test_prefab(125u);
  system.portcullis_blue_prefab = nacre_test_prefab(127u);
  system.objective_key_prefab = nacre_test_prefab(119u);
  system.objective_lever_prefab = nacre_test_prefab(120u);
  system.world_origin = vec3_make(8.0f, 0.0f, -3.0f);
  const u32 colored_before = nacre_test_colored_gate_spawns;
  system.regenerate = true;
  nacre_dungeon_system_update(&system, NULL, .016f);
  assert(system.generation_revision == 2u);
  assert(system.generated_entity_count > 500u);
  assert(system.missing_prefab_pieces == 0u);
  assert(system.planned_piece_count == system.generated_entity_count);
  assert(system.treasure_room_count >= 1 && system.treasure_room_count <= 3);
  assert(system.totem_count == system.treasure_room_count);
  assert(system.portcullis_count >= system.treasure_room_count);
  /* Every authored colored gate uses its color-specific prefab, regardless
   * of grid orientation; it is instantiated once, then rotated if needed. */
  assert(nacre_test_colored_gate_spawns - colored_before ==
      system.portcullis_count);
  assert(system.objectives_total == 3u);
  assert(nacre_test_alive == 1u + system.generated_entity_count * 2u);
  assert(nacre_test_animation_stops >= system.door_count + system.portcullis_count);
  assert(nacre_test_animation_plays == 0u); /* Never on scene creation. */
  /* The same authored X-facing prefab is used on both X and Z edges.
   * In a 50x50 grid, a 1m X edge has (half-integer X, integer Z), while
   * a Z edge has (integer X, half-integer Z). Its root must rotate +90°
   * about Y only in the latter case, with no translation or scaling hacks. */
  {
    unsigned x_walls = 0, z_walls = 0, x_passages = 0, z_passages = 0;
    unsigned x_gates = 0, z_gates = 0;
    unsigned x_walls_2m = 0, z_walls_2m = 0;
    for (u32 id = 1; id < nacre_test_next_id; ++id)
    {
      const NacreTestEntity *e = &nacre_test_entities[id];
      if (!e->alive || !e->spawned_prefab) continue;
      const u32 prefab = e->spawned_prefab;
      if (prefab != 102 && prefab != 104 && prefab != 106 && prefab != 114 &&
          prefab != 116 && prefab != 123 && prefab != 125 &&
          prefab != 127) continue;
      /* 2m walls are centered on an integer in BOTH axes, so their
       * orientation is determined by their resulting quaternion. */
      const Quat rotation = e->transform.local_rotation;
      if (prefab == 104u)
      {
        if (rotation.y > .5f) ++z_walls_2m;
        else ++x_walls_2m;
        continue;
      }
      const bool along_z = fabsf(e->transform.local_position.z -
          roundf(e->transform.local_position.z)) > .25f;
      Quat expected = prefab == 102u ?
          quat_axis_angle(vec3_make(1.0f, 0.0f, 0.0f), .2f) :
          (Quat){0, 0, 0, 1};
      if (along_z) expected = quat_mul(
          quat_axis_angle(vec3_make(0.0f, 1.0f, 0.0f), STDXM_PI * .5f),
          expected);
      assert(fabsf(rotation.x - expected.x) < .0001f);
      assert(fabsf(rotation.y - expected.y) < .0001f);
      assert(fabsf(rotation.z - expected.z) < .0001f);
      assert(fabsf(rotation.w - expected.w) < .0001f);
      if (prefab == 102) { if (along_z) ++z_walls; else ++x_walls; }
      if (prefab == 106) { if (along_z) ++z_passages; else ++x_passages; }
      if (prefab == 114 || prefab == 116 || prefab == 123 ||
          prefab == 125 || prefab == 127)
      { if (along_z) ++z_gates; else ++x_gates; }
    }
    assert(x_walls && z_walls && x_passages && z_passages);
    assert(x_gates && z_gates);
    assert(x_walls_2m && z_walls_2m);
  }

  u32 stable_count = nacre_test_alive;
  u32 stable_revision = system.generation_revision;
  {
    const NacreDungeonMap *map = nacre_dungeon_system_map_get();
    const uint8_t *identity = map->cells;
    unsigned tested = 0;
    /* Password known only after explicit totem discovery. One color maps
     * to multiple independent gate instances in the general case. */
    for(u32 p=0;p<map->passage_count;p++){
      const NacreDungeonPassage *gate=&map->passages[p];
      if(gate->kind!=NACRE_DUNGEON_PASSAGE_PORTCULLIS)continue;
      const NacreDungeonTotem *matching=NULL;
      for(u32 t=0;t<map->totem_count;t++)if(map->totems[t].id==gate->totem_id)
        matching=&map->totems[t];
      assert(matching);
      assert(!nacre_dungeon_system_portcullis_open(gate->a,gate->b,
          matching->sequence));
      NacreDungeonTotem revealed;
      assert(nacre_dungeon_system_totem_discover(matching->tile,&revealed));
      assert(revealed.id==matching->id && revealed.discovered);
      NacreDungeonDirection wrong[4];
      memcpy(wrong,matching->sequence,sizeof(wrong));
      wrong[0]=(NacreDungeonDirection)((wrong[0]+1)%4);
      assert(!nacre_dungeon_system_portcullis_open(gate->a,gate->b,wrong));
      const u32 spawned_before=nacre_test_spawn_calls;
      assert(nacre_dungeon_system_portcullis_open(gate->a,gate->b,
          matching->sequence));
      assert(nacre_test_spawn_calls == spawned_before); /* No prefab swap. */
      assert(nacre_test_animation_plays == 1u);
      assert(map->passages[p].opened);
      assert(!nacre_dungeon_system_portcullis_open(gate->a,gate->b,
          matching->sequence));
      break;
    }
    for(u32 p=0;p<map->passage_count;p++){
      const NacreDungeonPassage *door=&map->passages[p];
      if(door->kind!=NACRE_DUNGEON_PASSAGE_DOOR)continue;
      const u32 before = nacre_test_spawn_calls;
      assert(nacre_dungeon_system_door_open(door->a,door->b));
      assert(nacre_test_spawn_calls == before);
      assert(nacre_test_animation_plays == 2u);
      assert(map->passages[p].opened);
      break;
    }
    assert(!nacre_dungeon_system_exit_can_use(map->entry));
    for(u32 o=0;o<map->objective_count;o++){
      assert(nacre_dungeon_system_objective_complete(map->objectives[o].tile));
      assert(!nacre_dungeon_system_objective_complete(map->objectives[o].tile));
    }
    assert(system.objectives_completed==system.objectives_total);
    assert(nacre_dungeon_system_exit_can_use(map->exit));
    for (u32 i = 0; i < map->edge_count; ++i)
    {
      const NacreDungeonEdge edge = map->edges[i];
      if (edge.type != NACRE_DUNGEON_EDGE_CRACKED) continue;
      const u32 from = edge.a, to = edge.b;
      nacre_test_fail_on_spawn = nacre_test_spawn_calls + 1u;
      assert(!nacre_dungeon_system_cracked_wall_break(from, to));
      assert(nacre_dungeon_system_map_get()->edges[i].type == NACRE_DUNGEON_EDGE_CRACKED);
      assert(nacre_test_alive == stable_count);
      nacre_test_fail_on_spawn = 0u;
      const u32 spawn_before_break = nacre_test_spawn_calls;
      assert(nacre_dungeon_system_cracked_wall_break(from, to));
      /* The whole dungeon must NOT be re-instantiated for a wall hit. */
      assert(nacre_test_spawn_calls == spawn_before_break + 1u);
      map = nacre_dungeon_system_map_get();
      assert(map->cells == identity);
      assert(map->edges[i].type == NACRE_DUNGEON_EDGE_HOLE);
      assert(system.layout_valid);
      assert(nacre_test_alive == 1u + system.generated_entity_count * 2u);
      /* Repeat cannot break same wall twice. */
      assert(!nacre_dungeon_system_cracked_wall_break(from, to));
      tested++; break;
    }
    assert(tested);
    stable_count = nacre_test_alive;
    if (map->secret_room_count)
    {
      const NacreDungeonSecretRoom *secret = &map->secret_rooms[0];
      const u32 spawn_before_secret = nacre_test_spawn_calls;
      assert(nacre_dungeon_system_secret_open(
          secret->entrance_outside, secret->entrance_inside));
      assert(nacre_test_spawn_calls == spawn_before_secret + 1u);
      assert(nacre_test_alive == 1u + system.generated_entity_count * 2u);
      stable_count = nacre_test_alive;
    }
  }

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
  system.wall_1m_prefab = ldk_asset_prefab_null();
  system.wall_2m_prefab = ldk_asset_prefab_null();
  system.passage_prefab = ldk_asset_prefab_null();
  system.hole_prefab = ldk_asset_prefab_null();
  system.cracked_prefab = ldk_asset_prefab_null();
  system.pillar_intact_prefab = ldk_asset_prefab_null();
  system.pillar_broken_prefab = ldk_asset_prefab_null();
  system.door_prefab = ldk_asset_prefab_null();
  system.portcullis_prefab = ldk_asset_prefab_null();
  system.portcullis_red_prefab = ldk_asset_prefab_null();
  system.portcullis_green_prefab = ldk_asset_prefab_null();
  system.portcullis_blue_prefab = ldk_asset_prefab_null();
  system.totem_prefab = ldk_asset_prefab_null();
  system.entry_prefab = ldk_asset_prefab_null();
  system.exit_prefab = ldk_asset_prefab_null();
  system.objective_key_prefab = ldk_asset_prefab_null();
  system.objective_lever_prefab = ldk_asset_prefab_null();
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
