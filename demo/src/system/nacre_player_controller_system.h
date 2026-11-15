#ifndef NACRE_PLAYER_CONTROLLER_SYSTEM_H
#define NACRE_PLAYER_CONTROLLER_SYSTEM_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <module/ldk_entity.h>
#include <module/ldk_system.h>
#include <stdx/stdx_math.h>
#include <stdx/stdx_string.h>

/*
 * Nacre: grid-based character controller for dungeon/camera playtesting.
 *
 * No collision meshes, rigid bodies or raycasts are required.  A move is
 * accepted by nacre_dungeon_navigation_step() BEFORE any visual interpolation.
 * Consequently, a player cannot walk through a wall, a void tile, or a
 * currently unbroken cracked wall, even if the visual prefab was omitted.
 *
 * Controls: WASD move one tile at a time, hold to continue.  Q/E orbit the
 * existing MAIN camera by 90 degrees and rotate the movement frame with it.
 * In the initial orientation, the camera looks diagonally across the grid
 * (45 degrees yaw); W still means world -Z, D still means world +X, so
 * movement remains cardinal and follows the camera's 90-degree Q/E turns.
 *
 * This is intentionally NOT the general-purpose LDK CharacterController.
 */
int nacre_player_controller_system_initialize(void *data);
void nacre_player_controller_system_update(
    void *data, const LDKEntityGroup *group, float dt);
void nacre_player_controller_system_terminate(void *data);

//@system name=NacrePlayerControllerSystem initialize=nacre_player_controller_system_initialize update=nacre_player_controller_system_update terminate=nacre_player_controller_system_terminate order=30
typedef struct NacrePlayerControllerSystem
{
  //@begin_group "Player"
  /* Optional player visual prefab. Without it, a transform-only test entity
   * still exists and the camera follows it; the player will be invisible. */
  LDKAssetPrefab player_prefab;
  /* Animation time for one cardinal tile. Collision is always grid-based. */
  //@inspect min=0.03 max=1
  float step_duration;
  /* Extra Y added to the generated dungeon's floor height. */
  float ground_offset;
  //@end_group

  //@begin_group "Camera"
  /** Reuse the scene's first enabled camera with role MAIN. */
  bool follow_main_camera;
  /* Offset relative to player with zero quarter-turns; camera pivot at player. */
  Vec3 camera_offset;
  /* Look toward this height above the player's base. */
  //@inspect min=0
  float look_at_height;
  /* Camera translation response in 1/sec; 0 means immediate. */
  //@inspect min=0
  float follow_sharpness;
  /* Angular velocity for Q/E 90 degree orbit, in degrees per second. */
  //@inspect min=20
  float orbit_degrees_per_second;
  /* Additional yaw relative to the 45-degree diagonal camera framing.
   * Zero = diagonal; -45 = old axis-aligned framing. Q/E still add 90.
   * The default zero also works with previously serialized scenes. */
  //@inspect min=-180 max=180
  float camera_yaw_adjust_degrees;
  //@end_group

  //@begin_group "Runtime Debug"
  //@inspect readonly runtime
  bool player_spawned;
  //@inspect readonly runtime
  bool player_moving;
  //@inspect readonly runtime
  u32 grid_x;
  //@inspect readonly runtime
  u32 grid_z;
  //@inspect readonly runtime
  u32 successful_steps;
  //@inspect readonly runtime
  u32 blocked_steps;
  //@inspect readonly runtime
  i32 camera_quarter_turns;
  //@inspect readonly runtime
  XSmallstr status;
  //@end_group
} NacrePlayerControllerSystem;

#endif // NACRE_PLAYER_CONTROLLER_SYSTEM_H
