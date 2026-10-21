/* Editor-only creature authoring callbacks. Discovered by Comet; the LDK
 * Inspector contains no references to TFTF-specific components or controls. */
#include <ldk_game.h>
#include <generated_component_metadata.h>
#include <component/ldk_transform.h>
#include <module/ldk_component.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>

#include "../component/tftf_creature.h"
#include "../component/tftf_creature_node.h"
#include "../system/tftf_creature_system.h"
#include "../system/tftf_creature_chain.h"

#include <stdio.h>
#include <string.h>

typedef struct TFTFCreatureEditorNode
{
  LDKEntity entity;
  TFTFCreatureNodeComponent data;
  Vec3 position;
  Vec3 scale;
} TFTFCreatureEditorNode;

static u32 s_editor_nodes_get(LDKEntity creature,
    TFTFCreatureEditorNode *out, u32 capacity)
{
  const LDKTransform *parent = ldk_ecs_component_get_const(
      creature, LDK_COMPONENT_TYPE_TRANSFORM);
  if (parent == NULL)
  {
    return 0u;
  }
  u32 count = 0u;
  LDKEntity child = parent->first_child;
  while (!x_handle_is_null(child))
  {
    const LDKTransform *transform = ldk_ecs_component_get_const(
        child, LDK_COMPONENT_TYPE_TRANSFORM);
    if (transform == NULL)
    {
      break;
    }
    LDKEntity next = transform->next_sibling;
    const TFTFCreatureNodeComponent *node = ldk_ecs_component_get_const(
        child, ldk_component_type(TFTFCreatureNodeComponent));
    if (node != NULL && count < capacity)
    {
      u32 at = count;
      while (at > 0u && (out[at - 1u].data.order > node->order ||
          (out[at - 1u].data.order == node->order &&
           out[at - 1u].entity.index > child.index)))
      {
        out[at] = out[at - 1u];
        --at;
      }
      out[at].entity = child;
      out[at].data = *node;
      out[at].position = transform->local_position;
      out[at].scale = transform->local_scale;
      ++count;
    }
    child = next;
  }
  return count;
}


/* The ordinary LDK node gizmo edits local_position. In stopped editor mode
 * detect endpoint changes and solve the chain before rendering the preview.
 * There are no new editor gizmos, transforms or runtime-only dependencies. */
#define TFTF_EDITOR_CHAIN_CAPACITY 256u

typedef struct TFTFCreatureEditorChainCache
{
  bool seen;
  bool initialized;
  LDKEntity creature;
  u32 count;
  LDKEntity entities[TFTF_CREATURE_MAX_NODES];
  Vec3 previous[TFTF_CREATURE_MAX_NODES];
  TFTFCreatureChain chain;
} TFTFCreatureEditorChainCache;

static TFTFCreatureEditorChainCache s_chain_caches[TFTF_EDITOR_CHAIN_CAPACITY];

static bool s_editor_entity_equal(LDKEntity a, LDKEntity b)
{
  return a.index == b.index && a.version == b.version;
}

static TFTFCreatureEditorChainCache *s_editor_chain_cache(LDKEntity creature)
{
  TFTFCreatureEditorChainCache *unused = NULL;
  for (u32 i = 0u; i < TFTF_EDITOR_CHAIN_CAPACITY; ++i)
  {
    TFTFCreatureEditorChainCache *entry = &s_chain_caches[i];
    if (entry->initialized && s_editor_entity_equal(entry->creature, creature))
    {
      return entry;
    }
    if (!entry->initialized && !unused)
    {
      unused = entry;
    }
  }
  if (unused)
  {
    memset(unused, 0, sizeof(*unused));
    unused->creature = creature;
    unused->initialized = true;
  }
  return unused;
}

/* Estimate each node's local bone axis from its neighboring joints. */
static Vec3 s_editor_node_axis(const Vec3 *pose, u32 count, u32 i)
{
  const Vec3 delta = vec3_sub(
      pose[i + 1u < count ? i + 1u : i],
      pose[i > 0u ? i - 1u : i]);
  return vec3_len2(delta) > 1e-10f
      ? vec3_norm(delta) : vec3_make(0.0f, 1.0f, 0.0f);
}

static bool s_editor_chain_apply(LDKEntity entity, void *user)
{
  (void)user;
  if (!ldk_ecs_component_get_const(entity,
          ldk_component_type(TFTFCreatureComponent)) &&
      !ldk_ecs_component_get_const(entity,
          ldk_component_type(TFTFCreatureChainComponent)))
  {
    return true;
  }
  TFTFCreatureEditorNode nodes[TFTF_CREATURE_MAX_NODES];
  const u32 count = s_editor_nodes_get(entity, nodes, TFTF_CREATURE_MAX_NODES);
  if (count < 2u)
  {
    return true;
  }
  TFTFCreatureEditorChainCache *cache = s_editor_chain_cache(entity);
  if (!cache)
  {
    return true;
  }
  cache->seen = true;
  TFTFCreatureNode definition[TFTF_CREATURE_MAX_NODES] = {0};
  bool topology_changed = cache->count != count;
  for (u32 i = 0u; i < count; ++i)
  {
    definition[i].position = nodes[i].position;
    definition[i].scale = nodes[i].scale;
    definition[i].min_bend_degrees = nodes[i].data.min_bend_degrees;
    definition[i].max_bend_degrees = nodes[i].data.max_bend_degrees;
    if (!topology_changed && !s_editor_entity_equal(
            cache->entities[i], nodes[i].entity))
    {
      topology_changed = true;
    }
  }
  if (topology_changed)
  {
    tftf_creature_chain_build(&cache->chain, definition, count);
    cache->count = count;
    for (u32 i = 0u; i < count; ++i)
    {
      cache->entities[i] = nodes[i].entity;
      cache->previous[i] = nodes[i].position;
    }
    return true;
  }
  /* Refresh current joint constraints, but never recalculate existing link
   * lengths in response to an endpoint drag. */
  TFTFCreatureChain configured;
  tftf_creature_chain_build(&configured, definition, count);
  memcpy(cache->chain.bend_min, configured.bend_min,
      sizeof(cache->chain.bend_min));
  memcpy(cache->chain.bend_max, configured.bend_max,
      sizeof(cache->chain.bend_max));

  const float first_change = vec3_len2(vec3_sub(nodes[0].position,
      cache->previous[0]));
  const float last_change = vec3_len2(vec3_sub(nodes[count - 1u].position,
      cache->previous[count - 1u]));
  bool interior_changed = false;
  for (u32 i = 1u; i + 1u < count; ++i)
  {
    if (vec3_len2(vec3_sub(nodes[i].position, cache->previous[i])) > 1e-8f)
    {
      interior_changed = true;
    }
  }
  if (interior_changed)
  {
    /* Explicit repositioning of an interior point changes anatomy.
     * Treat it as a fresh rest pose instead of pretending it is IK. */
    cache->chain = configured;
  }
  else if (first_change > 1e-8f || last_change > 1e-8f)
  {
    const u32 moving = first_change > last_change ? 0u : count - 1u;
    TFTFCreaturePose pose;
    memcpy(pose.nodes, cache->previous,
        (size_t)count * sizeof(*pose.nodes));
    (void)tftf_creature_chain_solve(&cache->chain, &pose,
        nodes[moving].position, moving);
    /* The attached entities follow each bone's bend, not just its joint
     * position. Rotate by the change of centerline tangent, preserving
     * any manually authored orientation (including twist). */
    for (u32 i = 0u; i < count; ++i)
    {
      const Vec3 from = s_editor_node_axis(cache->previous, count, i);
      const Vec3 to = s_editor_node_axis(pose.nodes, count, i);
      const LDKTransform *tf = ldk_ecs_component_get_const(
          nodes[i].entity, LDK_COMPONENT_TYPE_TRANSFORM);
      if (tf && vec3_len2(vec3_sub(from, to)) > 1e-10f)
      {
        (void)ldk_transform_set_local_rotation(nodes[i].entity,
            quat_mul(quat_from_to(from, to), tf->local_rotation));
      }
      if (vec3_len2(vec3_sub(pose.nodes[i], nodes[i].position)) > 1e-10f)
      {
        (void)ldk_transform_set_local_position(nodes[i].entity,
            pose.nodes[i]);
      }
      nodes[i].position = pose.nodes[i];
    }
  }
  for (u32 i = 0u; i < count; ++i)
  {
    cache->previous[i] = nodes[i].position;
    cache->chain.rest[i] = nodes[i].position;
  }
  return true;
}

static void s_editor_chains_update(void)
{
  for (u32 i = 0u; i < TFTF_EDITOR_CHAIN_CAPACITY; ++i)
  {
    s_chain_caches[i].seen = false;
  }
  (void)ldk_ecs_entity_foreach(s_editor_chain_apply, NULL);
  for (u32 i = 0u; i < TFTF_EDITOR_CHAIN_CAPACITY; ++i)
  {
    if (!s_chain_caches[i].seen)
    {
      memset(&s_chain_caches[i], 0, sizeof(s_chain_caches[i]));
    }
  }
}

/* Insertion and removal keep order contiguous, so the reflected Order field
 * stays inside its supported 0..MAX-1 range after multiple edit operations. */
static void s_editor_nodes_renumber(const TFTFCreatureEditorNode *nodes,
    u32 count)
{
  for (u32 i = 0u; i < count; ++i)
  {
    TFTFCreatureNodeComponent *node = ldk_ecs_component_get(
        nodes[i].entity, ldk_component_type(TFTFCreatureNodeComponent));
    if (node != NULL)
    {
      node->order = i;
    }
  }
}

static bool s_editor_node_append(LDKEntity creature,
    TFTFCreatureBodyAxis axis)
{
  TFTFCreatureEditorNode nodes[TFTF_CREATURE_MAX_NODES];
  u32 count = s_editor_nodes_get(creature, nodes, TFTF_CREATURE_MAX_NODES);
  if (count >= TFTF_CREATURE_MAX_NODES)
  {
    return false;
  }
  s_editor_nodes_renumber(nodes, count);
  Vec3 position = vec3_make(0.0f, 0.0f, 0.0f);
  if (count >= 2u)
  {
    const Vec3 delta = vec3_sub(nodes[count - 1u].position,
        nodes[count - 2u].position);
    if (vec3_len2(delta) > 0.0025f)
    {
      position = vec3_add(nodes[count - 1u].position, delta);
    }
    else
    {
      position = nodes[count - 1u].position;
      if (axis == TFTF_CREATURE_BODY_VERTICAL)
      {
        position.y += 0.65f;
      }
      else
      {
        position.z -= 0.65f;
      }
    }
  }
  else if (count == 1u)
  {
    position = nodes[0].position;
    if (axis == TFTF_CREATURE_BODY_VERTICAL)
    {
      position.y += 0.65f;
    }
    else
    {
      position.z -= 0.65f;
    }
  }
  TFTFCreatureNode node = {0};
  node.position = position;
  node.scale = count ? nodes[count - 1u].scale :
      vec3_make(0.45f, 0.45f, 0.45f);
  node.min_bend_degrees = 0.0f;
  node.max_bend_degrees = 155.0f;
  return !x_handle_is_null(tftf_creature_node_create(creature, count, node));
}

static void s_editor_entity_subtree_destroy(LDKEntity entity)
{
  const LDKTransform *transform = ldk_ecs_component_get_const(
      entity, LDK_COMPONENT_TYPE_TRANSFORM);
  LDKEntity child = transform ? transform->first_child : x_handle_null();
  while (!x_handle_is_null(child))
  {
    const LDKTransform *child_transform = ldk_ecs_component_get_const(
        child, LDK_COMPONENT_TYPE_TRANSFORM);
    LDKEntity next = child_transform ? child_transform->next_sibling
        : x_handle_null();
    s_editor_entity_subtree_destroy(child);
    child = next;
  }
  ldk_ecs_entity_destroy(entity);
}

static void s_editor_node_remove(LDKEntity creature, u32 index)
{
  TFTFCreatureEditorNode nodes[TFTF_CREATURE_MAX_NODES];
  const u32 count = s_editor_nodes_get(creature, nodes,
      TFTF_CREATURE_MAX_NODES);
  if (index >= count)
  {
    return;
  }
  s_editor_entity_subtree_destroy(nodes[index].entity);
  u32 at = 0u;
  for (u32 i = 0u; i < count; ++i)
  {
    if (i != index)
    {
      nodes[at++] = nodes[i];
    }
  }
  s_editor_nodes_renumber(nodes, at);
}

/* Manual actions use the common void(LDKEntity, float) signature. */
//@editor_callback component=TFTFCreatureComponent flags=Manual label=Add_Body_Node
LDK_GAME_API void tftf_editor_add_body_node(LDKEntity entity, float dt)
{
  (void)dt;
  const TFTFCreatureComponent *creature = ldk_ecs_component_get_const(
      entity, ldk_component_type(TFTFCreatureComponent));
  if (creature)
  {
    (void)s_editor_node_append(entity, creature->body_axis);
  }
}

//@editor_callback component=TFTFCreatureComponent flags=Manual label=Remove_Last_Body_Node
LDK_GAME_API void tftf_editor_remove_last_body_node(LDKEntity entity, float dt)
{
  (void)dt;
  if (!ldk_ecs_component_get_const(entity,
          ldk_component_type(TFTFCreatureComponent)))
  {
    return;
  }
  TFTFCreatureEditorNode nodes[TFTF_CREATURE_MAX_NODES];
  u32 count = s_editor_nodes_get(entity, nodes, TFTF_CREATURE_MAX_NODES);
  if (count)
  {
    s_editor_node_remove(entity, count - 1u);
  }
}

//@editor_callback component=TFTFCreatureNodeComponent flags=Manual label=Remove_Body_Node
LDK_GAME_API void tftf_editor_remove_body_node(LDKEntity entity, float dt)
{
  (void)dt;
  const LDKEntity parent = ldk_scenegraph_get_parent(entity);
  if (x_handle_is_null(parent) ||
      (!ldk_ecs_component_get_const(parent,
          ldk_component_type(TFTFCreatureComponent)) &&
       !ldk_ecs_component_get_const(parent,
          ldk_component_type(TFTFCreatureChainComponent))))
  {
    return;
  }
  TFTFCreatureEditorNode nodes[TFTF_CREATURE_MAX_NODES];
  const u32 count = s_editor_nodes_get(parent, nodes, TFTF_CREATURE_MAX_NODES);
  for (u32 i = 0; i < count; ++i)
  {
    if (nodes[i].entity.index == entity.index &&
        nodes[i].entity.version == entity.version)
    {
      s_editor_node_remove(parent, i);
      return;
    }
  }
}

/* System-scope preview executes once per stopped editor frame, irrespective
 * of Inspector selection. It does not invoke the runtime ECS system. */
//@editor_callback system=TFTFCreatureSystem flags=OnEditorUpdate label=Creature_Preview
LDK_GAME_API void tftf_editor_creature_preview(
    u64 system_id, void *data, float dt)
{
  (void)system_id;
  (void)data;
  (void)dt;
  s_editor_chains_update();
  tftf_creature_editor_preview_update();
}

typedef struct TFTFCreatureEditorChainCount
{
  LDKEntity creature;
  u32 count;
} TFTFCreatureEditorChainCount;

static bool s_editor_chain_count_one(LDKEntity entity, void *user)
{
  TFTFCreatureEditorChainCount *counter = user;
  if (!ldk_ecs_component_get_const(entity,
          ldk_component_type(TFTFCreatureChainComponent)))
  {
    return true;
  }
  LDKEntity parent = ldk_scenegraph_get_parent(entity);
  for (u32 depth = 0u; depth <= 2u * TFTF_CREATURE_MAX_CHAIN_DEPTH + 2u;
       ++depth)
  {
    if (x_handle_is_null(parent))
    {
      break;
    }
    if (s_editor_entity_equal(parent, counter->creature))
    {
      ++counter->count;
      break;
    }
    parent = ldk_scenegraph_get_parent(parent);
  }
  return true;
}

/* Refuse changes that would silently exceed the renderer's chain limits. */
static bool s_editor_chain_can_add(LDKEntity node)
{
  LDKEntity owner = node;
  u32 depth = 0u;
  while (!x_handle_is_null(owner))
  {
    if (ldk_ecs_component_get_const(owner,
            ldk_component_type(TFTFCreatureComponent)))
    {
      TFTFCreatureEditorChainCount counter = {0};
      counter.creature = owner;
      (void)ldk_ecs_entity_foreach(s_editor_chain_count_one, &counter);
      return depth / 2u < TFTF_CREATURE_MAX_CHAIN_DEPTH &&
          counter.count < TFTF_CREATURE_MAX_CHAINS;
    }
    owner = ldk_scenegraph_get_parent(owner);
    ++depth;
  }
  return false;
}

/* A chain can be added under ANY node, including nodes belonging to
 * secondary chains. Regular LDK entities may also be reparented there. */
//@editor_callback component=TFTFCreatureNodeComponent flags=Manual label=Add_Chain
LDK_GAME_API void tftf_editor_add_chain(LDKEntity entity, float dt)
{
  (void)dt;
  if (ldk_ecs_component_get_const(entity,
          ldk_component_type(TFTFCreatureNodeComponent)) &&
      s_editor_chain_can_add(entity))
  {
    (void)tftf_creature_chain_create(entity);
  }
}

//@editor_callback component=TFTFCreatureChainComponent flags=Manual label=Add_Node
LDK_GAME_API void tftf_editor_add_chain_node(LDKEntity entity, float dt)
{
  (void)dt;
  if (ldk_ecs_component_get_const(entity,
          ldk_component_type(TFTFCreatureChainComponent)))
  {
    (void)s_editor_node_append(entity, TFTF_CREATURE_BODY_HORIZONTAL);
  }
}

//@editor_callback component=TFTFCreatureChainComponent flags=Manual label=Remove_Last_Node
LDK_GAME_API void tftf_editor_remove_last_chain_node(LDKEntity entity, float dt)
{
  (void)dt;
  if (!ldk_ecs_component_get_const(entity,
          ldk_component_type(TFTFCreatureChainComponent))) return;
  TFTFCreatureEditorNode nodes[TFTF_CREATURE_MAX_NODES];
  const u32 count = s_editor_nodes_get(entity, nodes, TFTF_CREATURE_MAX_NODES);
  if (count > 2u) s_editor_node_remove(entity, count - 1u);
}

//@editor_callback component=TFTFCreatureChainComponent flags=Manual label=Remove_Chain
LDK_GAME_API void tftf_editor_remove_chain(LDKEntity entity, float dt)
{
  (void)dt;
  if (ldk_ecs_component_get_const(entity,
          ldk_component_type(TFTFCreatureChainComponent)))
  {
    s_editor_entity_subtree_destroy(entity);
  }
}
