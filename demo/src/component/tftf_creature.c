#include "tftf_creature.h"
#include <ldk_material_asset.h>

#include <generated_component_metadata.h>
#include <ldk.h>
#include <module/ldk_component.h>
#include <module/ldk_ecs.h>

static TFTFCreatureComponent s_tftf_creature_defaults(void)
{
  TFTFCreatureComponent c = {0};
  c.enabled = true;
  c.body_axis = TFTF_CREATURE_BODY_VERTICAL;
  c.animation_speed = 1.0f;
  c.body_pulse = .035f;
  c.body_height = 0.0f;
  c.body_pulse_speed = 2.3f;
  c.body_pulse_waves = .28f;
  c.body_sway = .035f;
  c.body_sway_speed = 1.2f;
  c.radial_segments = 6u;
  c.segments_per_node = 3u;
  c.movement_speed = 0.45f;
  c.step_height = .30f;
  c.step_length = .65f;
  c.sting_speed = 1.0f;
  c.sting_hold_time = 0.0f;
  c.sting_return_speed = 1.0f;
  c.sting_interval = 0.0f;
  c.sting_target = vec3_make(0.0f, 0.0f, -1.5f);
  c.appendage_distribution = TFTF_CREATURE_APPENDAGE_BILATERAL;
  c.appendage_spacing = 0.65f;
  c.appendage_root_offset = 0.0f;
  c.appendage_length = 1.3f;
  c.appendage_reach = .95f;
  c.appendage_knee = .45f;
  c.appendage_tip_taper = .12f;
  c.appendage_thickness = 0.16f;
  c.body_color = 0x877466ffu;
  c.appendage_color = 0x706957ffu;
  c.body_material = ldk_asset_material_null();
  c.appendage_material = ldk_asset_material_null();
  return c;
}

TFTFCreatureComponent tftf_creature_make_worm(void)
{
  TFTFCreatureComponent c = s_tftf_creature_defaults();
  c.motion = TFTF_CREATURE_MOTION_STING;
  c.node_count = 6u;
  c.nodes[0] = (TFTFCreatureNode){
      {0.0f, 0.2f, 0.0f}, {0.45f, 0.45f, 0.45f}, 0.0f, 155.0f};
  c.nodes[1] = (TFTFCreatureNode){
      {0.0f, 1.03f, 0.3f}, {0.41f, 0.41f, 0.41f}, 0.0f, 155.0f};
  c.nodes[2] = (TFTFCreatureNode){
      {0.0f, 1.96f, 0.3f}, {0.45f, 0.45f, 0.45f}, 0.0f, 155.0f};
  c.nodes[3] = (TFTFCreatureNode){
      {0.0f, 2.8f, -0.1f}, {0.47f, 0.47f, 0.47f}, 0.0f, 155.0f};
  c.nodes[4] = (TFTFCreatureNode){
      {0.0f, 2.88f, -0.95f}, {0.4f, 0.4f, 0.4f}, 0.0f, 155.0f};
  c.nodes[5] = (TFTFCreatureNode){
      {0.0f, 2.24f, -1.18f}, {0.32f, 0.32f, 0.32f}, 0.0f, 155.0f};
  return c;
}

TFTFCreatureComponent tftf_creature_make_snail(void)
{
  TFTFCreatureComponent c = s_tftf_creature_defaults();
  c.motion = TFTF_CREATURE_MOTION_CRAWL;
  c.body_axis = TFTF_CREATURE_BODY_HORIZONTAL;
  c.node_count = 5u;
  c.nodes[0] = (TFTFCreatureNode){
      {0.0f, 0.4f, 0.9f}, {0.42f, 0.42f, 0.42f}, 0.0f, 155.0f};
  c.nodes[1] = (TFTFCreatureNode){
      {0.0f, 0.5f, 0.42f}, {0.56f, 0.56f, 0.56f}, 0.0f, 155.0f};
  c.nodes[2] = (TFTFCreatureNode){
      {0.0f, 0.45f, -0.1f}, {0.48f, 0.48f, 0.48f}, 0.0f, 155.0f};
  c.nodes[3] = (TFTFCreatureNode){
      {0.0f, 0.58f, -0.7f}, {0.34f, 0.34f, 0.34f}, 0.0f, 155.0f};
  c.nodes[4] = (TFTFCreatureNode){
      {0.0f, 0.8f, -1.1f}, {0.24f, 0.24f, 0.24f}, 0.0f, 155.0f};
  return c;
}

TFTFCreatureComponent tftf_creature_make_legged(void)
{
  TFTFCreatureComponent c = tftf_creature_make_snail();
  c.motion = TFTF_CREATURE_MOTION_WALK;
  c.body_sway = .07f;
  c.appendage_type = TFTF_CREATURE_APPENDAGE_LEG;
  c.appendage_count = 6u;
  c.appendage_length = 1.25f;
  c.appendage_reach = 1.0f;
  c.appendage_thickness = .18f;
  c.appendage_knee = .55f;
  c.step_height = .40f;
  c.step_length = .8f;
  c.body_height = .5f;
  c.body_pulse = .025f;
  return c;
}

static bool s_tftf_creature_attach(
    LDKEntityRegistry *entities, LDKComponentRegistry *registry,
    LDKEntity entity, void *component, u32 component_index,
    const void *initial_value, void *user)
{
  TFTFCreatureComponent *creature = component;
  (void)registry;
  (void)entity;
  (void)component_index;
  (void)user;
  if (!entities || !creature)
  {
    return false;
  }
  if (!initial_value)
  {
    /* Authored in the editor from an empty chain, not a preset. */
    *creature = s_tftf_creature_defaults();
  }
  return true;
}

bool tftf_creature_component_register(void)
{
  LDKECS *ecs = ldk_module_get(LDK_MODULE_ECS);
  if (!ecs)
  {
    return false;
  }
  LDKComponentDesc desc = {0};
  desc.name = "TFTFCreatureComponent";
  desc.type = ldk_component_type(TFTFCreatureComponent);
  desc.entry_size = sizeof(TFTFCreatureComponent);
  desc.initial_capacity = 16u;
  desc.attach = s_tftf_creature_attach;
  if (ldk_component_is_registered(&ecs->component, desc.type))
  {
    return true;
  }
  return ldk_ecs_component_register(&desc);
}
