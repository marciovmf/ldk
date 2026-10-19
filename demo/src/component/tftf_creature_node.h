#ifndef TFTF_CREATURE_NODE_H
#define TFTF_CREATURE_NODE_H

#include "tftf_creature.h"
#include <module/ldk_entity.h>

/* One body control point is an ordinary child entity of the creature.
 * Its LDKTransform.local_position defines the center, and local_scale
 * defines the three ellipsoid semiaxes in creature coordinates.
 * This makes the anatomy editable and serializable with the existing LDK
 * Hierarchy and Inspector; no support for struct arrays is required. */
//@component
typedef struct TFTFCreatureNodeComponent
{
  //@begin_group "Body Node"
  //@inspect min=0 max=7
  u32 order;
  //@inspect min=0 max=175
  float min_bend_degrees;
  //@inspect min=0 max=175
  float max_bend_degrees;
  //@end_group
} TFTFCreatureNodeComponent;

/* Attachments belong to a node entity, so changing the node order never
 * unexpectedly transfers its eye or fang to another node. */
//@component
typedef struct TFTFCreatureNodeAttachmentComponent
{
  //@begin_group "Attachment"
  TFTFCreatureAttachmentType type;
  //@inspect min=0.01
  float size;
  //@inspect min=-180 max=180
  float orbit_azimuth_degrees;
  //@inspect min=-89 max=89
  float orbit_elevation_degrees;
  //@inspect min=-180 max=180
  float yaw_degrees;
  //@inspect min=-89 max=89
  float pitch_degrees;
  //@inspect min=-180 max=180
  float roll_degrees;
  //@end_group
} TFTFCreatureNodeAttachmentComponent;

/* Four editable volume controls beneath a tentacle entity, itself parented
 * to a body node. The same skin generator is used for body and tentacle. */
#define TFTF_CREATURE_TENTACLE_NODE_COUNT 4u
#define TFTF_CREATURE_MAX_TENTACLES 32u

//@component
typedef struct TFTFCreatureTentacleComponent
{
  //@begin_group "Tentacle"
  bool enabled;
  //@end_group
} TFTFCreatureTentacleComponent;

/* Same volume controls as the body, but a distinct component keeps
 * body-specific editor actions off tentacle nodes. */
//@component
typedef struct TFTFCreatureTentacleNodeComponent
{
  //@begin_group "Tentacle Node"
  //@inspect min=0 max=3
  u32 order;
  //@inspect min=0 max=175
  float min_bend_degrees;
  //@inspect min=0 max=175
  float max_bend_degrees;
  //@end_group
} TFTFCreatureTentacleNodeComponent;

bool tftf_creature_node_component_register(void);
LDKEntity tftf_creature_tentacle_create(LDKEntity body_node);
LDKEntity tftf_creature_node_create(
    LDKEntity creature, u32 order, TFTFCreatureNode node);
/* Convert the inline preset anatomy to editable child entities. */
bool tftf_creature_nodes_create_from_component(
    LDKEntity creature, const TFTFCreatureComponent *component);

#endif // TFTF_CREATURE_NODE_H
