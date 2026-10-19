#include "tftf_creature_node.h"

#include <component/ldk_transform.h>
#include <generated_component_metadata.h>
#include <ldk.h>
#include <module/ldk_component.h>
#include <module/ldk_ecs.h>
#include <module/ldk_scenegraph.h>

#include <stdio.h>

static bool s_tftf_creature_node_attach(
    LDKEntityRegistry *entities, LDKComponentRegistry *registry,
    LDKEntity entity, void *component, u32 component_index,
    const void *initial_value, void *user)
{
  TFTFCreatureNodeComponent *node = component;
  (void)registry;
  (void)entity;
  (void)component_index;
  (void)user;
  if (!entities || !node)
  {
    return false;
  }
  if (!initial_value)
  {
    *node = (TFTFCreatureNodeComponent){0};
    node->max_bend_degrees = 155.0f;
  }
  return true;
}

static bool s_tftf_creature_attachment_attach(
    LDKEntityRegistry *entities, LDKComponentRegistry *registry,
    LDKEntity entity, void *component, u32 component_index,
    const void *initial_value, void *user)
{
  TFTFCreatureNodeAttachmentComponent *a = component;
  (void)registry;
  (void)entity;
  (void)component_index;
  (void)user;
  if (!entities || !a)
  {
    return false;
  }
  if (!initial_value)
  {
    *a = (TFTFCreatureNodeAttachmentComponent){0};
    a->type = TFTF_CREATURE_ATTACHMENT_EYE;
    a->size = .2f;
  }
  return true;
}

static bool s_tftf_creature_tentacle_attach(
    LDKEntityRegistry *entities, LDKComponentRegistry *registry,
    LDKEntity entity, void *component, u32 component_index,
    const void *initial_value, void *user)
{
  TFTFCreatureTentacleComponent *tentacle = component;
  (void)registry;
  (void)entity;
  (void)component_index;
  (void)user;
  if (!entities || !tentacle)
  {
    return false;
  }
  if (!initial_value)
  {
    *tentacle = (TFTFCreatureTentacleComponent){0};
    tentacle->enabled = true;
  }
  return true;
}

static bool s_tftf_creature_tentacle_node_attach(
    LDKEntityRegistry *entities, LDKComponentRegistry *registry,
    LDKEntity entity, void *component, u32 component_index,
    const void *initial_value, void *user)
{
  TFTFCreatureTentacleNodeComponent *node = component;
  (void)registry;
  (void)entity;
  (void)component_index;
  (void)user;
  if (!entities || !node)
  {
    return false;
  }
  if (!initial_value)
  {
    *node = (TFTFCreatureTentacleNodeComponent){0};
    node->max_bend_degrees = 155.0f;
  }
  return true;
}

bool tftf_creature_node_component_register(void)
{
  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  if (!ecs)
  {
    return false;
  }
  LDKComponentDesc desc = {0};
  desc.name = "TFTFCreatureNodeComponent";
  desc.type = ldk_component_type(TFTFCreatureNodeComponent);
  desc.entry_size = sizeof(TFTFCreatureNodeComponent);
  desc.initial_capacity = 64u;
  desc.attach = s_tftf_creature_node_attach;
  if (!ldk_component_is_registered(&ecs->component, desc.type) &&
      !ldk_ecs_component_register(&desc))
  {
    return false;
  }
  LDKComponentDesc attachment = {0};
  attachment.name = "TFTFCreatureNodeAttachmentComponent";
  attachment.type = ldk_component_type(TFTFCreatureNodeAttachmentComponent);
  attachment.entry_size = sizeof(TFTFCreatureNodeAttachmentComponent);
  attachment.initial_capacity = 64u;
  attachment.attach = s_tftf_creature_attachment_attach;
  if (!ldk_component_is_registered(&ecs->component, attachment.type) &&
      !ldk_ecs_component_register(&attachment))
  {
    return false;
  }
  LDKComponentDesc tentacle = {0};
  tentacle.name = "TFTFCreatureTentacleComponent";
  tentacle.type = ldk_component_type(TFTFCreatureTentacleComponent);
  tentacle.entry_size = sizeof(TFTFCreatureTentacleComponent);
  tentacle.initial_capacity = 64u;
  tentacle.attach = s_tftf_creature_tentacle_attach;
  if (!ldk_component_is_registered(&ecs->component, tentacle.type) &&
      !ldk_ecs_component_register(&tentacle))
  {
    return false;
  }
  LDKComponentDesc tentacle_node = {0};
  tentacle_node.name = "TFTFCreatureTentacleNodeComponent";
  tentacle_node.type = ldk_component_type(TFTFCreatureTentacleNodeComponent);
  tentacle_node.entry_size = sizeof(TFTFCreatureTentacleNodeComponent);
  tentacle_node.initial_capacity = 128u;
  tentacle_node.attach = s_tftf_creature_tentacle_node_attach;
  return ldk_component_is_registered(&ecs->component, tentacle_node.type) ||
      ldk_ecs_component_register(&tentacle_node);
}

LDKEntity tftf_creature_node_create(
    LDKEntity creature, u32 order, TFTFCreatureNode node)
{
  LDKEntity invalid = x_handle_null();
  LDKEntity child = ldk_ecs_entity_create();
  if (x_handle_is_null(child))
  {
    return invalid;
  }
  TFTFCreatureNodeComponent data = {0};
  data.order = order;
  data.min_bend_degrees = node.min_bend_degrees;
  data.max_bend_degrees = node.max_bend_degrees;
  if (!ldk_scenegraph_set_parent(child, creature) ||
      !ldk_transform_set_local_position(child, node.position) ||
      !ldk_transform_set_local_scale(child, node.scale) ||
      !ldk_ecs_component_add(child,
          ldk_component_type(TFTFCreatureNodeComponent), &data))
  {
    ldk_ecs_entity_destroy(child);
    return invalid;
  }
  char name[32];
  snprintf(name, sizeof(name), "Body Node %u", order);
  (void)ldk_ecs_entity_name_set(child, name);
  return child;
}

static LDKEntity s_tftf_creature_attachment_create(
    LDKEntity node, const TFTFCreatureAttachment *source)
{
  LDKEntity invalid = x_handle_null();
  if (!source)
  {
    return invalid;
  }
  LDKEntity entity = ldk_ecs_entity_create();
  if (x_handle_is_null(entity))
  {
    return invalid;
  }
  TFTFCreatureNodeAttachmentComponent data = {0};
  data.type = source->type;
  data.size = source->size;
  data.orbit_azimuth_degrees = source->orbit_azimuth_degrees;
  data.orbit_elevation_degrees = source->orbit_elevation_degrees;
  data.yaw_degrees = source->yaw_degrees;
  data.pitch_degrees = source->pitch_degrees;
  data.roll_degrees = source->roll_degrees;
  if (!ldk_scenegraph_set_parent(entity, node) ||
      !ldk_ecs_component_add(entity,
          ldk_component_type(TFTFCreatureNodeAttachmentComponent), &data))
  {
    ldk_ecs_entity_destroy(entity);
    return invalid;
  }
  (void)ldk_ecs_entity_name_set(entity,
      data.type == TFTF_CREATURE_ATTACHMENT_EYE ? "Eye" : "Fang");
  return entity;
}

bool tftf_creature_nodes_create_from_component(
    LDKEntity creature, const TFTFCreatureComponent *component)
{
  if (!component || component->node_count < 2u ||
      component->node_count > TFTF_CREATURE_MAX_NODES)
  {
    return false;
  }
  LDKEntity nodes[TFTF_CREATURE_MAX_NODES];
  u32 created = 0;
  bool success = true;
  for (u32 i = 0; i < component->node_count; ++i)
  {
    nodes[i] = tftf_creature_node_create(creature, i, component->nodes[i]);
    if (x_handle_is_null(nodes[i]))
    {
      success = false;
      break;
    }
    ++created;
  }
  if (success)
  {
    for (u32 i = 0; i < component->attachment_count; ++i)
    {
      const TFTFCreatureAttachment *a = &component->attachments[i];
      if (a->node_index >= created ||
          x_handle_is_null(s_tftf_creature_attachment_create(
              nodes[a->node_index], a)))
      {
        success = false;
        break;
      }
    }
  }
  if (!success)
  {
    for (u32 i = 0; i < created; ++i)
    {
      const LDKTransform *transform = ldk_ecs_component_get_const(
          nodes[i], LDK_COMPONENT_TYPE_TRANSFORM);
      LDKEntity child = transform ? transform->first_child : x_handle_null();
      while (!x_handle_is_null(child))
      {
        const LDKTransform *ct = ldk_ecs_component_get_const(
            child, LDK_COMPONENT_TYPE_TRANSFORM);
        LDKEntity next = ct ? ct->next_sibling : x_handle_null();
        ldk_ecs_entity_destroy(child);
        child = next;
      }
      ldk_ecs_entity_destroy(nodes[i]);
    }
  }
  return success;
}

/* The control points are ordinary nodes in a four-node child chain. Their
 * Transform scales define the sphere volumes, like body nodes. */
LDKEntity tftf_creature_tentacle_create(LDKEntity body_node)
{
  const TFTFCreatureNodeComponent *parent_node = ldk_ecs_component_get_const(
      body_node, ldk_component_type(TFTFCreatureNodeComponent));
  if (!parent_node)
  {
    return x_handle_null();
  }
  LDKEntity tentacle = ldk_ecs_entity_create();
  if (x_handle_is_null(tentacle))
  {
    return tentacle;
  }
  const TFTFCreatureTentacleComponent data = {.enabled = true};
  if (!ldk_scenegraph_set_parent(tentacle, body_node) ||
      !ldk_ecs_component_add(tentacle,
          ldk_component_type(TFTFCreatureTentacleComponent), &data) ||
      !ldk_transform_set_local_position(tentacle,
          vec3_make(.85f, 0.0f, 0.0f)))
  {
    ldk_ecs_entity_destroy(tentacle);
    return x_handle_null();
  }
  (void)ldk_ecs_entity_name_set(tentacle, "Tentacle");
  const Vec3 positions[TFTF_CREATURE_TENTACLE_NODE_COUNT] = {
      {0.0f, 0.0f, 0.0f},
      {.80f, .16f, 0.0f},
      {1.62f, .02f, 0.0f},
      {2.30f, -.12f, 0.0f}};
  const float radii[TFTF_CREATURE_TENTACLE_NODE_COUNT] = {
      .68f, .43f, .22f, .035f};
  LDKEntity nodes[TFTF_CREATURE_TENTACLE_NODE_COUNT];
  u32 created = 0u;
  for (u32 i = 0u; i < TFTF_CREATURE_TENTACLE_NODE_COUNT; ++i)
  {
    nodes[i] = ldk_ecs_entity_create();
    if (!x_handle_is_null(nodes[i]))
    {
      TFTFCreatureTentacleNodeComponent node = {0};
      node.order = i;
      node.max_bend_degrees = 155.0f;
      if (!ldk_scenegraph_set_parent(nodes[i], tentacle) ||
          !ldk_transform_set_local_position(nodes[i], positions[i]) ||
          !ldk_transform_set_local_scale(nodes[i],
              vec3_make(radii[i], radii[i], radii[i])) ||
          !ldk_ecs_component_add(nodes[i],
              ldk_component_type(TFTFCreatureTentacleNodeComponent), &node))
      {
        ldk_ecs_entity_destroy(nodes[i]);
        nodes[i] = x_handle_null();
      }
    }
    if (x_handle_is_null(nodes[i]))
    {
      for (u32 j = 0u; j < created; ++j)
      {
        ldk_ecs_entity_destroy(nodes[j]);
      }
      ldk_ecs_entity_destroy(tentacle);
      return x_handle_null();
    }
    char name[32];
    snprintf(name, sizeof(name), "Tentacle Node %u", i);
    (void)ldk_ecs_entity_name_set(nodes[i], name);
    ++created;
  }
  return tentacle;
}
