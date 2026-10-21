#include "tftf_creature_node.h"

#include <component/ldk_transform.h>
#include <generated_component_metadata.h>
#include <ldk.h>
#include <module/ldk_component.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>
#include <stdio.h>

static bool s_node_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *registry, LDKEntity entity, void *component,
    u32 component_index, const void *initial_value, void *user)
{
  (void)registry;
  (void)entity;
  (void)component_index;
  (void)user;
  if (!entities || !component) return false;
  if (!initial_value)
  {
    TFTFCreatureNodeComponent *node = component;
    *node = (TFTFCreatureNodeComponent){0};
    node->max_bend_degrees = 155.0f;
  }
  return true;
}

static bool s_chain_attach(LDKEntityRegistry *entities,
    LDKComponentRegistry *registry, LDKEntity entity, void *component,
    u32 component_index, const void *initial_value, void *user)
{
  (void)registry;
  (void)entity;
  (void)component_index;
  (void)user;
  if (!entities || !component) return false;
  if (!initial_value)
  {
    TFTFCreatureChainComponent *chain = component;
    *chain = (TFTFCreatureChainComponent){0};
    chain->enabled = true;
    chain->start_cap = TFTF_CREATURE_CAP_ROUNDED;
    chain->end_cap = TFTF_CREATURE_CAP_POINTED;
  }
  return true;
}

bool tftf_creature_node_component_register(void)
{
  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  if (!ecs) return false;
  LDKComponentDesc desc = {0};
  desc.name = "TFTFCreatureNodeComponent";
  desc.type = ldk_component_type(TFTFCreatureNodeComponent);
  desc.entry_size = sizeof(TFTFCreatureNodeComponent);
  desc.initial_capacity = 128u;
  desc.attach = s_node_attach;
  if (!ldk_component_is_registered(&ecs->component, desc.type) &&
      !ldk_ecs_component_register(&desc)) return false;
  desc = (LDKComponentDesc){0};
  desc.name = "TFTFCreatureChainComponent";
  desc.type = ldk_component_type(TFTFCreatureChainComponent);
  desc.entry_size = sizeof(TFTFCreatureChainComponent);
  desc.initial_capacity = 64u;
  desc.attach = s_chain_attach;
  return ldk_component_is_registered(&ecs->component, desc.type) ||
      ldk_ecs_component_register(&desc);
}

LDKEntity tftf_creature_node_create(
    LDKEntity owner, u32 order, TFTFCreatureNode node)
{
  LDKEntity child = ldk_ecs_entity_create();
  if (x_handle_is_null(child)) return child;
  TFTFCreatureNodeComponent data = {0};
  data.order = order;
  data.min_bend_degrees = node.min_bend_degrees;
  data.max_bend_degrees = node.max_bend_degrees;
  if (!ldk_scenegraph_set_parent(child, owner) ||
      !ldk_transform_set_local_position(child, node.position) ||
      !ldk_transform_set_local_scale(child, node.scale) ||
      !ldk_transform_set_local_rotation(child, quat_id()) ||
      !ldk_ecs_component_add(child,
          ldk_component_type(TFTFCreatureNodeComponent), &data))
  {
    ldk_ecs_entity_destroy(child);
    return x_handle_null();
  }
  char name[32];
  snprintf(name, sizeof(name), "Node %u", order);
  (void)ldk_ecs_entity_name_set(child, name);
  return child;
}

bool tftf_creature_nodes_create_from_component(
    LDKEntity creature, const TFTFCreatureComponent *component)
{
  if (!component || component->node_count < 2u ||
      component->node_count > TFTF_CREATURE_MAX_NODES) return false;
  LDKEntity nodes[TFTF_CREATURE_MAX_NODES];
  u32 created = 0u;
  for (u32 i = 0u; i < component->node_count; ++i)
  {
    nodes[i] = tftf_creature_node_create(creature, i, component->nodes[i]);
    if (x_handle_is_null(nodes[i]))
    {
      for (u32 j = 0u; j < created; ++j) ldk_ecs_entity_destroy(nodes[j]);
      return false;
    }
    ++created;
  }
  return true;
}

LDKEntity tftf_creature_chain_create(LDKEntity parent_node)
{
  if (!ldk_ecs_component_get_const(parent_node,
          ldk_component_type(TFTFCreatureNodeComponent)))
    return x_handle_null();
  LDKEntity entity = ldk_ecs_entity_create();
  if (x_handle_is_null(entity)) return entity;
  const TFTFCreatureChainComponent data = {
      .enabled = true,
      .start_cap = TFTF_CREATURE_CAP_ROUNDED,
      .end_cap = TFTF_CREATURE_CAP_POINTED};
  if (!ldk_scenegraph_set_parent(entity, parent_node) ||
      !ldk_transform_set_local_position(entity, vec3_make(0, 0, 0)) ||
      !ldk_transform_set_local_rotation(entity, quat_id()) ||
      !ldk_transform_set_local_scale(entity, vec3_make(1, 1, 1)) ||
      !ldk_ecs_component_add(entity,
          ldk_component_type(TFTFCreatureChainComponent), &data))
  {
    ldk_ecs_entity_destroy(entity);
    return x_handle_null();
  }
  (void)ldk_ecs_entity_name_set(entity, "Chain");
  /* Small initial chain; same nodes as the body, same FABRIK/editor tools. */
  const Vec3 p[] = {
      {0.0f, 0.0f, 0.0f}, {.75f, .0f, 0.0f},
      {1.5f, .0f, 0.0f}, {2.2f, .0f, 0.0f}};
  const float r[] = {.38f, .26f, .14f, .02f};
  for (u32 i = 0u; i < 4u; ++i)
  {
    TFTFCreatureNode node = {0};
    node.position = p[i];
    node.scale = vec3_make(r[i], r[i], r[i]);
    node.max_bend_degrees = 155.0f;
    if (x_handle_is_null(tftf_creature_node_create(entity, i, node)))
    {
      /* Remove the complete partial hierarchy. */
      const LDKTransform *tf = ldk_ecs_component_get_const(
          entity, LDK_COMPONENT_TYPE_TRANSFORM);
      LDKEntity child = tf ? tf->first_child : x_handle_null();
      while (!x_handle_is_null(child))
      {
        const LDKTransform *ct = ldk_ecs_component_get_const(
            child, LDK_COMPONENT_TYPE_TRANSFORM);
        const LDKEntity next = ct ? ct->next_sibling : x_handle_null();
        ldk_ecs_entity_destroy(child);
        child = next;
      }
      ldk_ecs_entity_destroy(entity);
      return x_handle_null();
    }
  }
  return entity;
}
