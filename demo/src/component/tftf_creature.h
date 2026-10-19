#ifndef TFTF_CREATURE_H
#define TFTF_CREATURE_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <stdx/stdx_math.h>

#define TFTF_CREATURE_MAX_NODES 8u
#define TFTF_CREATURE_MAX_ATTACHMENTS 16u
#define TFTF_CREATURE_MAX_APPENDAGES 8u

//@enum
typedef enum TFTFCreatureBodyAxis
{
  TFTF_CREATURE_BODY_VERTICAL = 0,
  TFTF_CREATURE_BODY_HORIZONTAL
} TFTFCreatureBodyAxis;

//@enum
typedef enum TFTFCreatureMotion
{
  TFTF_CREATURE_MOTION_IDLE = 0,
  TFTF_CREATURE_MOTION_CRAWL,
  TFTF_CREATURE_MOTION_WALK,
  TFTF_CREATURE_MOTION_STING
} TFTFCreatureMotion;

/* Defaults retain the last node as the head in existing saved scenes. */
//@enum
typedef enum TFTFCreatureHead
{
  TFTF_CREATURE_HEAD_LAST = 0,
  TFTF_CREATURE_HEAD_FIRST,
  TFTF_CREATURE_HEAD_NONE
} TFTFCreatureHead;

//@enum
typedef enum TFTFCreatureEndCap
{
  TFTF_CREATURE_CAP_ROUNDED = 0,
  TFTF_CREATURE_CAP_POINTED
} TFTFCreatureEndCap;

//@enum
typedef enum TFTFCreatureAppendageType
{
  TFTF_CREATURE_APPENDAGE_NONE = 0,
  TFTF_CREATURE_APPENDAGE_LEG,
  TFTF_CREATURE_APPENDAGE_TENTACLE
} TFTFCreatureAppendageType;

//@enum
typedef enum TFTFCreatureAppendageDistribution
{
  TFTF_CREATURE_APPENDAGE_BILATERAL = 0,
  TFTF_CREATURE_APPENDAGE_RADIAL
} TFTFCreatureAppendageDistribution;

//@enum
typedef enum TFTFCreatureAttachmentType
{
  TFTF_CREATURE_ATTACHMENT_EYE = 0,
  TFTF_CREATURE_ATTACHMENT_FANG
} TFTFCreatureAttachmentType;

/* Anatomical coordinates are relative to the creature entity. Node order,
 * not height, defines the continuous body chain. */
typedef struct TFTFCreatureNode
{
  Vec3 position;
  /* Ellipsoid semiaxes in the creature's local coordinate frame. */
  Vec3 scale;
  float min_bend_degrees;
  float max_bend_degrees;
} TFTFCreatureNode;

typedef struct TFTFCreatureAttachment
{
  TFTFCreatureAttachmentType type;
  u32 node_index;
  float size;
  float orbit_azimuth_degrees;
  float orbit_elevation_degrees;
  float yaw_degrees;
  float pitch_degrees;
  float roll_degrees;
} TFTFCreatureAttachment;

//@component
typedef struct TFTFCreatureComponent
{
  //@begin_group "General"
  bool enabled;
  TFTFCreatureMotion motion;
  TFTFCreatureBodyAxis body_axis;
  //@inspect min=0
  float animation_speed;
  //@end_group

  //@begin_group "Body"
  //@inspect min=0
  float body_pulse;
  //@inspect min=-2 max=4
  float body_height;
  //@inspect min=0
  float body_pulse_speed;
  //@inspect min=0
  float body_pulse_waves;
  //@inspect min=0
  float body_sway;
  //@inspect min=0
  float body_sway_speed;
  TFTFCreatureHead head;
  TFTFCreatureEndCap start_cap;
  TFTFCreatureEndCap end_cap;
  //@inspect hidden
  u32 node_count;
  //@inspect min=4 max=16
  u32 radial_segments;
  //@inspect min=1 max=10
  u32 segments_per_node;
  /* Fallback preset anatomy. When the creature has child entities with
   * TFTFCreatureNodeComponent, those are authoritative and editable in the
   * scene Hierarchy + Inspector. The fixed array is not serialized by Comet. */
  TFTFCreatureNode nodes[TFTF_CREATURE_MAX_NODES];
  //@end_group

  //@begin_group "Movement"
  /* Walk locomotion speed, in world units/second. This is the only
   * locomotion-speed control: foot cadence and swing speed are derived. */
  //@inspect min=0
  float movement_speed;
  //@inspect min=0
  float step_height;
  //@inspect min=0
  float step_length;
  //@inspect min=0
  float wave_amplitude;
  //@inspect min=0
  float wave_frequency;
  //@inspect min=0
  float wave_cycles;
  /* Forward strike speed multiplier; 1 = 1.98 seconds to reach the target.
   * Zero freezes the forward motion. */
  //@inspect min=0
  float sting_speed;
  /* Time spent at the target before retracting, in seconds (animation time).
   * This is independent of both forward and return speeds. */
  //@inspect min=0
  float sting_hold_time;
  /* Return speed multiplier; 1 = 2.42 seconds to return to the rest pose.
   * Zero freezes the return motion. */
  //@inspect min=0
  float sting_return_speed;
  /* Seconds spent idle after the head returns, before the next attack.
   * 0 preserves the original continuous attack cycle. This wait is
   * affected by animation_speed, but not by the strike or return speeds. */
  //@inspect min=0
  float sting_interval;
  /* Fallback local-space target when no player is present, and in editor preview. */
  Vec3 sting_target;
  //@end_group

  //@begin_group "Appendages"
  TFTFCreatureAppendageType appendage_type;
  TFTFCreatureAppendageDistribution appendage_distribution;
  //@inspect min=0 max=8
  u32 appendage_count;
  //@inspect min=0
  float appendage_spacing;
  /* Signed distance from the ellipsoid skin to the appendage root.
   * Zero attaches at the surface; negative values inset the root. */
  float appendage_root_offset;
  //@inspect min=0
  float appendage_length;
  //@inspect min=0
  float appendage_reach;
  //@inspect min=0
  float appendage_knee;
  //@inspect min=0 max=1
  float appendage_tip_taper;
  //@inspect min=0
  float appendage_thickness;
  //@end_group

  /* Automatic locomotor tentacles are four volume controls, from the
   * attachment at the body (node 0) to the free tip (node 3).
   * These are actual radii in creature-local units, independent of the
   * thickness/taper settings used by conventional legs. */
  //@begin_group "Tentacle Volumes"
  //@inspect min=0.005
  float tentacle_node_0_radius;
  //@inspect min=0.005
  float tentacle_node_1_radius;
  //@inspect min=0.005
  float tentacle_node_2_radius;
  //@inspect min=0.005
  float tentacle_node_3_radius;
  //@end_group

  //@begin_group "Attachments"
  //@inspect min=0 max=16
  u32 attachment_count;
  TFTFCreatureAttachment attachments[TFTF_CREATURE_MAX_ATTACHMENTS];
  //@end_group

  //@begin_group "Materials"
  LDKAssetMaterial body_material;
  LDKAssetMaterial appendage_material;
  LDKAssetMaterial eye_material;
  LDKAssetMaterial fang_material;
  //@end_group

  //@begin_group "Appearance"
  //@inspect widget=COLOR
  u32 body_color;
  //@inspect widget=COLOR
  u32 appendage_color;
  //@inspect widget=COLOR
  u32 eye_color;
  //@inspect widget=COLOR
  u32 fang_color;
  //@end_group
} TFTFCreatureComponent;

TFTFCreatureComponent tftf_creature_make_worm(void);
TFTFCreatureComponent tftf_creature_make_snail(void);
TFTFCreatureComponent tftf_creature_make_tentacled(void);
TFTFCreatureComponent tftf_creature_make_legged(void);
bool tftf_creature_component_register(void);

#endif // TFTF_CREATURE_H
