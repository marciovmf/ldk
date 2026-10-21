#ifndef TFTF_CREATURE_NODE_H
#define TFTF_CREATURE_NODE_H

#include "tftf_creature.h"
#include <module/ldk_entity.h>

/* Nodes are ordinary LDK entities, parented directly to a creature or chain.
 * Position specifies the rest joint position; scale is the ellipsoid volume.
 * Other entities parented to these nodes inherit their animated transform. */
//@component
typedef struct TFTFCreatureNodeComponent
{
  //@begin_group "Chain Node"
  //@inspect min=0 max=7
  u32 order;
  //@inspect min=0 max=175
  float min_bend_degrees;
  //@inspect min=0 max=175
  float max_bend_degrees;
  //@end_group
} TFTFCreatureNodeComponent;

/* A chain is an entity parented to any creature node. Its own children are
 * regular TFTFCreatureNodeComponent entities. Nesting is supported. */
//@component
typedef struct TFTFCreatureChainComponent
{
  //@begin_group "Chain"
  bool enabled;
  TFTFCreatureEndCap start_cap;
  TFTFCreatureEndCap end_cap;
  //@end_group
} TFTFCreatureChainComponent;

#define TFTF_CREATURE_MAX_CHAINS 32u
#define TFTF_CREATURE_MAX_CHAIN_DEPTH 8u

bool tftf_creature_node_component_register(void);
LDKEntity tftf_creature_chain_create(LDKEntity parent_node);
LDKEntity tftf_creature_node_create(
    LDKEntity chain_owner, u32 order, TFTFCreatureNode node);
bool tftf_creature_nodes_create_from_component(
    LDKEntity creature, const TFTFCreatureComponent *component);

#endif // TFTF_CREATURE_NODE_H
