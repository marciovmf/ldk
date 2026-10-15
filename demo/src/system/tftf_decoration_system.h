#ifndef TFTF_DECORATION_SYSTEM_H
#define TFTF_DECORATION_SYSTEM_H

#include <ldk_asset.h>
#include <ldk_common.h>
#include <module/ldk_system.h>

int tftf_decoration_system_initialize(void *data);
void tftf_decoration_system_update(
    void *data, const LDKEntityGroup *group, float dt);
void tftf_decoration_system_terminate(void *data);

//@system components=LDK_COMPONENT_TFTFPlayerCharacterComponent initialize=tftf_decoration_system_initialize update=tftf_decoration_system_update terminate=tftf_decoration_system_terminate flags=LDK_SYSTEM_FLAG_ENABLED|LDK_SYSTEM_FLAG_RUN_WHEN_PAUSED bucket=LDK_SYSTEM_BUCKET_RENDER order=10
typedef struct TFTFDecorationSystem
{
  //@begin_group "General"
  //@inspect min=0
  u32 radius;
  //@end_group

  //@begin_group "Decoration Types"
  //@begin_group "Deep Water"
  //@begin_group "Slot 0"
  LDKAssetMesh deep_water_decoration_0_mesh;
  LDKAssetMaterial deep_water_decoration_0_material;
  //@inspect min=0
  u32 deep_water_decoration_0_min_count;
  //@inspect min=0
  u32 deep_water_decoration_0_max_count;
  //@inspect min=0.01
  float deep_water_decoration_0_min_scale;
  //@inspect min=0.01
  float deep_water_decoration_0_max_scale;
  //@end_group

  //@begin_group "Slot 1"
  LDKAssetMesh deep_water_decoration_1_mesh;
  LDKAssetMaterial deep_water_decoration_1_material;
  //@inspect min=0
  u32 deep_water_decoration_1_min_count;
  //@inspect min=0
  u32 deep_water_decoration_1_max_count;
  //@inspect min=0.01
  float deep_water_decoration_1_min_scale;
  //@inspect min=0.01
  float deep_water_decoration_1_max_scale;
  //@end_group

  //@begin_group "Slot 2"
  LDKAssetMesh deep_water_decoration_2_mesh;
  LDKAssetMaterial deep_water_decoration_2_material;
  //@inspect min=0
  u32 deep_water_decoration_2_min_count;
  //@inspect min=0
  u32 deep_water_decoration_2_max_count;
  //@inspect min=0.01
  float deep_water_decoration_2_min_scale;
  //@inspect min=0.01
  float deep_water_decoration_2_max_scale;
  //@end_group

  //@begin_group "Slot 3"
  LDKAssetMesh deep_water_decoration_3_mesh;
  LDKAssetMaterial deep_water_decoration_3_material;
  //@inspect min=0
  u32 deep_water_decoration_3_min_count;
  //@inspect min=0
  u32 deep_water_decoration_3_max_count;
  //@inspect min=0.01
  float deep_water_decoration_3_min_scale;
  //@inspect min=0.01
  float deep_water_decoration_3_max_scale;
  //@end_group

  //@begin_group "Slot 4"
  LDKAssetMesh deep_water_decoration_4_mesh;
  LDKAssetMaterial deep_water_decoration_4_material;
  //@inspect min=0
  u32 deep_water_decoration_4_min_count;
  //@inspect min=0
  u32 deep_water_decoration_4_max_count;
  //@inspect min=0.01
  float deep_water_decoration_4_min_scale;
  //@inspect min=0.01
  float deep_water_decoration_4_max_scale;
  //@end_group

  //@begin_group "Slot 5"
  LDKAssetMesh deep_water_decoration_5_mesh;
  LDKAssetMaterial deep_water_decoration_5_material;
  //@inspect min=0
  u32 deep_water_decoration_5_min_count;
  //@inspect min=0
  u32 deep_water_decoration_5_max_count;
  //@inspect min=0.01
  float deep_water_decoration_5_min_scale;
  //@inspect min=0.01
  float deep_water_decoration_5_max_scale;
  //@end_group

  //@begin_group "Slot 6"
  LDKAssetMesh deep_water_decoration_6_mesh;
  LDKAssetMaterial deep_water_decoration_6_material;
  //@inspect min=0
  u32 deep_water_decoration_6_min_count;
  //@inspect min=0
  u32 deep_water_decoration_6_max_count;
  //@inspect min=0.01
  float deep_water_decoration_6_min_scale;
  //@inspect min=0.01
  float deep_water_decoration_6_max_scale;
  //@end_group

  //@begin_group "Slot 7"
  LDKAssetMesh deep_water_decoration_7_mesh;
  LDKAssetMaterial deep_water_decoration_7_material;
  //@inspect min=0
  u32 deep_water_decoration_7_min_count;
  //@inspect min=0
  u32 deep_water_decoration_7_max_count;
  //@inspect min=0.01
  float deep_water_decoration_7_min_scale;
  //@inspect min=0.01
  float deep_water_decoration_7_max_scale;
  //@end_group

  //@end_group

  //@begin_group "Shallow Water"
  //@begin_group "Slot 0"
  LDKAssetMesh shallow_water_decoration_0_mesh;
  LDKAssetMaterial shallow_water_decoration_0_material;
  //@inspect min=0
  u32 shallow_water_decoration_0_min_count;
  //@inspect min=0
  u32 shallow_water_decoration_0_max_count;
  //@inspect min=0.01
  float shallow_water_decoration_0_min_scale;
  //@inspect min=0.01
  float shallow_water_decoration_0_max_scale;
  //@end_group

  //@begin_group "Slot 1"
  LDKAssetMesh shallow_water_decoration_1_mesh;
  LDKAssetMaterial shallow_water_decoration_1_material;
  //@inspect min=0
  u32 shallow_water_decoration_1_min_count;
  //@inspect min=0
  u32 shallow_water_decoration_1_max_count;
  //@inspect min=0.01
  float shallow_water_decoration_1_min_scale;
  //@inspect min=0.01
  float shallow_water_decoration_1_max_scale;
  //@end_group

  //@begin_group "Slot 2"
  LDKAssetMesh shallow_water_decoration_2_mesh;
  LDKAssetMaterial shallow_water_decoration_2_material;
  //@inspect min=0
  u32 shallow_water_decoration_2_min_count;
  //@inspect min=0
  u32 shallow_water_decoration_2_max_count;
  //@inspect min=0.01
  float shallow_water_decoration_2_min_scale;
  //@inspect min=0.01
  float shallow_water_decoration_2_max_scale;
  //@end_group

  //@begin_group "Slot 3"
  LDKAssetMesh shallow_water_decoration_3_mesh;
  LDKAssetMaterial shallow_water_decoration_3_material;
  //@inspect min=0
  u32 shallow_water_decoration_3_min_count;
  //@inspect min=0
  u32 shallow_water_decoration_3_max_count;
  //@inspect min=0.01
  float shallow_water_decoration_3_min_scale;
  //@inspect min=0.01
  float shallow_water_decoration_3_max_scale;
  //@end_group

  //@begin_group "Slot 4"
  LDKAssetMesh shallow_water_decoration_4_mesh;
  LDKAssetMaterial shallow_water_decoration_4_material;
  //@inspect min=0
  u32 shallow_water_decoration_4_min_count;
  //@inspect min=0
  u32 shallow_water_decoration_4_max_count;
  //@inspect min=0.01
  float shallow_water_decoration_4_min_scale;
  //@inspect min=0.01
  float shallow_water_decoration_4_max_scale;
  //@end_group

  //@begin_group "Slot 5"
  LDKAssetMesh shallow_water_decoration_5_mesh;
  LDKAssetMaterial shallow_water_decoration_5_material;
  //@inspect min=0
  u32 shallow_water_decoration_5_min_count;
  //@inspect min=0
  u32 shallow_water_decoration_5_max_count;
  //@inspect min=0.01
  float shallow_water_decoration_5_min_scale;
  //@inspect min=0.01
  float shallow_water_decoration_5_max_scale;
  //@end_group

  //@begin_group "Slot 6"
  LDKAssetMesh shallow_water_decoration_6_mesh;
  LDKAssetMaterial shallow_water_decoration_6_material;
  //@inspect min=0
  u32 shallow_water_decoration_6_min_count;
  //@inspect min=0
  u32 shallow_water_decoration_6_max_count;
  //@inspect min=0.01
  float shallow_water_decoration_6_min_scale;
  //@inspect min=0.01
  float shallow_water_decoration_6_max_scale;
  //@end_group

  //@begin_group "Slot 7"
  LDKAssetMesh shallow_water_decoration_7_mesh;
  LDKAssetMaterial shallow_water_decoration_7_material;
  //@inspect min=0
  u32 shallow_water_decoration_7_min_count;
  //@inspect min=0
  u32 shallow_water_decoration_7_max_count;
  //@inspect min=0.01
  float shallow_water_decoration_7_min_scale;
  //@inspect min=0.01
  float shallow_water_decoration_7_max_scale;
  //@end_group

  //@end_group

  //@begin_group "Shore"
  //@begin_group "Slot 0"
  LDKAssetMesh shore_decoration_0_mesh;
  LDKAssetMaterial shore_decoration_0_material;
  //@inspect min=0
  u32 shore_decoration_0_min_count;
  //@inspect min=0
  u32 shore_decoration_0_max_count;
  //@inspect min=0.01
  float shore_decoration_0_min_scale;
  //@inspect min=0.01
  float shore_decoration_0_max_scale;
  //@end_group

  //@begin_group "Slot 1"
  LDKAssetMesh shore_decoration_1_mesh;
  LDKAssetMaterial shore_decoration_1_material;
  //@inspect min=0
  u32 shore_decoration_1_min_count;
  //@inspect min=0
  u32 shore_decoration_1_max_count;
  //@inspect min=0.01
  float shore_decoration_1_min_scale;
  //@inspect min=0.01
  float shore_decoration_1_max_scale;
  //@end_group

  //@begin_group "Slot 2"
  LDKAssetMesh shore_decoration_2_mesh;
  LDKAssetMaterial shore_decoration_2_material;
  //@inspect min=0
  u32 shore_decoration_2_min_count;
  //@inspect min=0
  u32 shore_decoration_2_max_count;
  //@inspect min=0.01
  float shore_decoration_2_min_scale;
  //@inspect min=0.01
  float shore_decoration_2_max_scale;
  //@end_group

  //@begin_group "Slot 3"
  LDKAssetMesh shore_decoration_3_mesh;
  LDKAssetMaterial shore_decoration_3_material;
  //@inspect min=0
  u32 shore_decoration_3_min_count;
  //@inspect min=0
  u32 shore_decoration_3_max_count;
  //@inspect min=0.01
  float shore_decoration_3_min_scale;
  //@inspect min=0.01
  float shore_decoration_3_max_scale;
  //@end_group

  //@begin_group "Slot 4"
  LDKAssetMesh shore_decoration_4_mesh;
  LDKAssetMaterial shore_decoration_4_material;
  //@inspect min=0
  u32 shore_decoration_4_min_count;
  //@inspect min=0
  u32 shore_decoration_4_max_count;
  //@inspect min=0.01
  float shore_decoration_4_min_scale;
  //@inspect min=0.01
  float shore_decoration_4_max_scale;
  //@end_group

  //@begin_group "Slot 5"
  LDKAssetMesh shore_decoration_5_mesh;
  LDKAssetMaterial shore_decoration_5_material;
  //@inspect min=0
  u32 shore_decoration_5_min_count;
  //@inspect min=0
  u32 shore_decoration_5_max_count;
  //@inspect min=0.01
  float shore_decoration_5_min_scale;
  //@inspect min=0.01
  float shore_decoration_5_max_scale;
  //@end_group

  //@begin_group "Slot 6"
  LDKAssetMesh shore_decoration_6_mesh;
  LDKAssetMaterial shore_decoration_6_material;
  //@inspect min=0
  u32 shore_decoration_6_min_count;
  //@inspect min=0
  u32 shore_decoration_6_max_count;
  //@inspect min=0.01
  float shore_decoration_6_min_scale;
  //@inspect min=0.01
  float shore_decoration_6_max_scale;
  //@end_group

  //@begin_group "Slot 7"
  LDKAssetMesh shore_decoration_7_mesh;
  LDKAssetMaterial shore_decoration_7_material;
  //@inspect min=0
  u32 shore_decoration_7_min_count;
  //@inspect min=0
  u32 shore_decoration_7_max_count;
  //@inspect min=0.01
  float shore_decoration_7_min_scale;
  //@inspect min=0.01
  float shore_decoration_7_max_scale;
  //@end_group

  //@end_group

  //@begin_group "Dry"
  //@begin_group "Slot 0"
  LDKAssetMesh dry_decoration_0_mesh;
  LDKAssetMaterial dry_decoration_0_material;
  //@inspect min=0
  u32 dry_decoration_0_min_count;
  //@inspect min=0
  u32 dry_decoration_0_max_count;
  //@inspect min=0.01
  float dry_decoration_0_min_scale;
  //@inspect min=0.01
  float dry_decoration_0_max_scale;
  //@end_group

  //@begin_group "Slot 1"
  LDKAssetMesh dry_decoration_1_mesh;
  LDKAssetMaterial dry_decoration_1_material;
  //@inspect min=0
  u32 dry_decoration_1_min_count;
  //@inspect min=0
  u32 dry_decoration_1_max_count;
  //@inspect min=0.01
  float dry_decoration_1_min_scale;
  //@inspect min=0.01
  float dry_decoration_1_max_scale;
  //@end_group

  //@begin_group "Slot 2"
  LDKAssetMesh dry_decoration_2_mesh;
  LDKAssetMaterial dry_decoration_2_material;
  //@inspect min=0
  u32 dry_decoration_2_min_count;
  //@inspect min=0
  u32 dry_decoration_2_max_count;
  //@inspect min=0.01
  float dry_decoration_2_min_scale;
  //@inspect min=0.01
  float dry_decoration_2_max_scale;
  //@end_group

  //@begin_group "Slot 3"
  LDKAssetMesh dry_decoration_3_mesh;
  LDKAssetMaterial dry_decoration_3_material;
  //@inspect min=0
  u32 dry_decoration_3_min_count;
  //@inspect min=0
  u32 dry_decoration_3_max_count;
  //@inspect min=0.01
  float dry_decoration_3_min_scale;
  //@inspect min=0.01
  float dry_decoration_3_max_scale;
  //@end_group

  //@begin_group "Slot 4"
  LDKAssetMesh dry_decoration_4_mesh;
  LDKAssetMaterial dry_decoration_4_material;
  //@inspect min=0
  u32 dry_decoration_4_min_count;
  //@inspect min=0
  u32 dry_decoration_4_max_count;
  //@inspect min=0.01
  float dry_decoration_4_min_scale;
  //@inspect min=0.01
  float dry_decoration_4_max_scale;
  //@end_group

  //@begin_group "Slot 5"
  LDKAssetMesh dry_decoration_5_mesh;
  LDKAssetMaterial dry_decoration_5_material;
  //@inspect min=0
  u32 dry_decoration_5_min_count;
  //@inspect min=0
  u32 dry_decoration_5_max_count;
  //@inspect min=0.01
  float dry_decoration_5_min_scale;
  //@inspect min=0.01
  float dry_decoration_5_max_scale;
  //@end_group

  //@begin_group "Slot 6"
  LDKAssetMesh dry_decoration_6_mesh;
  LDKAssetMaterial dry_decoration_6_material;
  //@inspect min=0
  u32 dry_decoration_6_min_count;
  //@inspect min=0
  u32 dry_decoration_6_max_count;
  //@inspect min=0.01
  float dry_decoration_6_min_scale;
  //@inspect min=0.01
  float dry_decoration_6_max_scale;
  //@end_group

  //@begin_group "Slot 7"
  LDKAssetMesh dry_decoration_7_mesh;
  LDKAssetMaterial dry_decoration_7_material;
  //@inspect min=0
  u32 dry_decoration_7_min_count;
  //@inspect min=0
  u32 dry_decoration_7_max_count;
  //@inspect min=0.01
  float dry_decoration_7_min_scale;
  //@inspect min=0.01
  float dry_decoration_7_max_scale;
  //@end_group

  //@end_group

  //@begin_group "Grass"
  //@begin_group "Slot 0"
  LDKAssetMesh grass_decoration_0_mesh;
  LDKAssetMaterial grass_decoration_0_material;
  //@inspect min=0
  u32 grass_decoration_0_min_count;
  //@inspect min=0
  u32 grass_decoration_0_max_count;
  //@inspect min=0.01
  float grass_decoration_0_min_scale;
  //@inspect min=0.01
  float grass_decoration_0_max_scale;
  //@end_group

  //@begin_group "Slot 1"
  LDKAssetMesh grass_decoration_1_mesh;
  LDKAssetMaterial grass_decoration_1_material;
  //@inspect min=0
  u32 grass_decoration_1_min_count;
  //@inspect min=0
  u32 grass_decoration_1_max_count;
  //@inspect min=0.01
  float grass_decoration_1_min_scale;
  //@inspect min=0.01
  float grass_decoration_1_max_scale;
  //@end_group

  //@begin_group "Slot 2"
  LDKAssetMesh grass_decoration_2_mesh;
  LDKAssetMaterial grass_decoration_2_material;
  //@inspect min=0
  u32 grass_decoration_2_min_count;
  //@inspect min=0
  u32 grass_decoration_2_max_count;
  //@inspect min=0.01
  float grass_decoration_2_min_scale;
  //@inspect min=0.01
  float grass_decoration_2_max_scale;
  //@end_group

  //@begin_group "Slot 3"
  LDKAssetMesh grass_decoration_3_mesh;
  LDKAssetMaterial grass_decoration_3_material;
  //@inspect min=0
  u32 grass_decoration_3_min_count;
  //@inspect min=0
  u32 grass_decoration_3_max_count;
  //@inspect min=0.01
  float grass_decoration_3_min_scale;
  //@inspect min=0.01
  float grass_decoration_3_max_scale;
  //@end_group

  //@begin_group "Slot 4"
  LDKAssetMesh grass_decoration_4_mesh;
  LDKAssetMaterial grass_decoration_4_material;
  //@inspect min=0
  u32 grass_decoration_4_min_count;
  //@inspect min=0
  u32 grass_decoration_4_max_count;
  //@inspect min=0.01
  float grass_decoration_4_min_scale;
  //@inspect min=0.01
  float grass_decoration_4_max_scale;
  //@end_group

  //@begin_group "Slot 5"
  LDKAssetMesh grass_decoration_5_mesh;
  LDKAssetMaterial grass_decoration_5_material;
  //@inspect min=0
  u32 grass_decoration_5_min_count;
  //@inspect min=0
  u32 grass_decoration_5_max_count;
  //@inspect min=0.01
  float grass_decoration_5_min_scale;
  //@inspect min=0.01
  float grass_decoration_5_max_scale;
  //@end_group

  //@begin_group "Slot 6"
  LDKAssetMesh grass_decoration_6_mesh;
  LDKAssetMaterial grass_decoration_6_material;
  //@inspect min=0
  u32 grass_decoration_6_min_count;
  //@inspect min=0
  u32 grass_decoration_6_max_count;
  //@inspect min=0.01
  float grass_decoration_6_min_scale;
  //@inspect min=0.01
  float grass_decoration_6_max_scale;
  //@end_group

  //@begin_group "Slot 7"
  LDKAssetMesh grass_decoration_7_mesh;
  LDKAssetMaterial grass_decoration_7_material;
  //@inspect min=0
  u32 grass_decoration_7_min_count;
  //@inspect min=0
  u32 grass_decoration_7_max_count;
  //@inspect min=0.01
  float grass_decoration_7_min_scale;
  //@inspect min=0.01
  float grass_decoration_7_max_scale;
  //@end_group

  //@end_group

  //@begin_group "Forest"
  //@begin_group "Slot 0"
  LDKAssetMesh forest_decoration_0_mesh;
  LDKAssetMaterial forest_decoration_0_material;
  //@inspect min=0
  u32 forest_decoration_0_min_count;
  //@inspect min=0
  u32 forest_decoration_0_max_count;
  //@inspect min=0.01
  float forest_decoration_0_min_scale;
  //@inspect min=0.01
  float forest_decoration_0_max_scale;
  //@end_group

  //@begin_group "Slot 1"
  LDKAssetMesh forest_decoration_1_mesh;
  LDKAssetMaterial forest_decoration_1_material;
  //@inspect min=0
  u32 forest_decoration_1_min_count;
  //@inspect min=0
  u32 forest_decoration_1_max_count;
  //@inspect min=0.01
  float forest_decoration_1_min_scale;
  //@inspect min=0.01
  float forest_decoration_1_max_scale;
  //@end_group

  //@begin_group "Slot 2"
  LDKAssetMesh forest_decoration_2_mesh;
  LDKAssetMaterial forest_decoration_2_material;
  //@inspect min=0
  u32 forest_decoration_2_min_count;
  //@inspect min=0
  u32 forest_decoration_2_max_count;
  //@inspect min=0.01
  float forest_decoration_2_min_scale;
  //@inspect min=0.01
  float forest_decoration_2_max_scale;
  //@end_group

  //@begin_group "Slot 3"
  LDKAssetMesh forest_decoration_3_mesh;
  LDKAssetMaterial forest_decoration_3_material;
  //@inspect min=0
  u32 forest_decoration_3_min_count;
  //@inspect min=0
  u32 forest_decoration_3_max_count;
  //@inspect min=0.01
  float forest_decoration_3_min_scale;
  //@inspect min=0.01
  float forest_decoration_3_max_scale;
  //@end_group

  //@begin_group "Slot 4"
  LDKAssetMesh forest_decoration_4_mesh;
  LDKAssetMaterial forest_decoration_4_material;
  //@inspect min=0
  u32 forest_decoration_4_min_count;
  //@inspect min=0
  u32 forest_decoration_4_max_count;
  //@inspect min=0.01
  float forest_decoration_4_min_scale;
  //@inspect min=0.01
  float forest_decoration_4_max_scale;
  //@end_group

  //@begin_group "Slot 5"
  LDKAssetMesh forest_decoration_5_mesh;
  LDKAssetMaterial forest_decoration_5_material;
  //@inspect min=0
  u32 forest_decoration_5_min_count;
  //@inspect min=0
  u32 forest_decoration_5_max_count;
  //@inspect min=0.01
  float forest_decoration_5_min_scale;
  //@inspect min=0.01
  float forest_decoration_5_max_scale;
  //@end_group

  //@begin_group "Slot 6"
  LDKAssetMesh forest_decoration_6_mesh;
  LDKAssetMaterial forest_decoration_6_material;
  //@inspect min=0
  u32 forest_decoration_6_min_count;
  //@inspect min=0
  u32 forest_decoration_6_max_count;
  //@inspect min=0.01
  float forest_decoration_6_min_scale;
  //@inspect min=0.01
  float forest_decoration_6_max_scale;
  //@end_group

  //@begin_group "Slot 7"
  LDKAssetMesh forest_decoration_7_mesh;
  LDKAssetMaterial forest_decoration_7_material;
  //@inspect min=0
  u32 forest_decoration_7_min_count;
  //@inspect min=0
  u32 forest_decoration_7_max_count;
  //@inspect min=0.01
  float forest_decoration_7_min_scale;
  //@inspect min=0.01
  float forest_decoration_7_max_scale;
  //@end_group

  //@end_group

  //@begin_group "Thorns"
  //@begin_group "Slot 0"
  LDKAssetMesh thorns_decoration_0_mesh;
  LDKAssetMaterial thorns_decoration_0_material;
  //@inspect min=0
  u32 thorns_decoration_0_min_count;
  //@inspect min=0
  u32 thorns_decoration_0_max_count;
  //@inspect min=0.01
  float thorns_decoration_0_min_scale;
  //@inspect min=0.01
  float thorns_decoration_0_max_scale;
  //@end_group

  //@begin_group "Slot 1"
  LDKAssetMesh thorns_decoration_1_mesh;
  LDKAssetMaterial thorns_decoration_1_material;
  //@inspect min=0
  u32 thorns_decoration_1_min_count;
  //@inspect min=0
  u32 thorns_decoration_1_max_count;
  //@inspect min=0.01
  float thorns_decoration_1_min_scale;
  //@inspect min=0.01
  float thorns_decoration_1_max_scale;
  //@end_group

  //@begin_group "Slot 2"
  LDKAssetMesh thorns_decoration_2_mesh;
  LDKAssetMaterial thorns_decoration_2_material;
  //@inspect min=0
  u32 thorns_decoration_2_min_count;
  //@inspect min=0
  u32 thorns_decoration_2_max_count;
  //@inspect min=0.01
  float thorns_decoration_2_min_scale;
  //@inspect min=0.01
  float thorns_decoration_2_max_scale;
  //@end_group

  //@begin_group "Slot 3"
  LDKAssetMesh thorns_decoration_3_mesh;
  LDKAssetMaterial thorns_decoration_3_material;
  //@inspect min=0
  u32 thorns_decoration_3_min_count;
  //@inspect min=0
  u32 thorns_decoration_3_max_count;
  //@inspect min=0.01
  float thorns_decoration_3_min_scale;
  //@inspect min=0.01
  float thorns_decoration_3_max_scale;
  //@end_group

  //@begin_group "Slot 4"
  LDKAssetMesh thorns_decoration_4_mesh;
  LDKAssetMaterial thorns_decoration_4_material;
  //@inspect min=0
  u32 thorns_decoration_4_min_count;
  //@inspect min=0
  u32 thorns_decoration_4_max_count;
  //@inspect min=0.01
  float thorns_decoration_4_min_scale;
  //@inspect min=0.01
  float thorns_decoration_4_max_scale;
  //@end_group

  //@begin_group "Slot 5"
  LDKAssetMesh thorns_decoration_5_mesh;
  LDKAssetMaterial thorns_decoration_5_material;
  //@inspect min=0
  u32 thorns_decoration_5_min_count;
  //@inspect min=0
  u32 thorns_decoration_5_max_count;
  //@inspect min=0.01
  float thorns_decoration_5_min_scale;
  //@inspect min=0.01
  float thorns_decoration_5_max_scale;
  //@end_group

  //@begin_group "Slot 6"
  LDKAssetMesh thorns_decoration_6_mesh;
  LDKAssetMaterial thorns_decoration_6_material;
  //@inspect min=0
  u32 thorns_decoration_6_min_count;
  //@inspect min=0
  u32 thorns_decoration_6_max_count;
  //@inspect min=0.01
  float thorns_decoration_6_min_scale;
  //@inspect min=0.01
  float thorns_decoration_6_max_scale;
  //@end_group

  //@begin_group "Slot 7"
  LDKAssetMesh thorns_decoration_7_mesh;
  LDKAssetMaterial thorns_decoration_7_material;
  //@inspect min=0
  u32 thorns_decoration_7_min_count;
  //@inspect min=0
  u32 thorns_decoration_7_max_count;
  //@inspect min=0.01
  float thorns_decoration_7_min_scale;
  //@inspect min=0.01
  float thorns_decoration_7_max_scale;
  //@end_group

  //@end_group

  //@begin_group "Mountain"
  //@begin_group "Slot 0"
  LDKAssetMesh mountain_decoration_0_mesh;
  LDKAssetMaterial mountain_decoration_0_material;
  //@inspect min=0
  u32 mountain_decoration_0_min_count;
  //@inspect min=0
  u32 mountain_decoration_0_max_count;
  //@inspect min=0.01
  float mountain_decoration_0_min_scale;
  //@inspect min=0.01
  float mountain_decoration_0_max_scale;
  //@end_group

  //@begin_group "Slot 1"
  LDKAssetMesh mountain_decoration_1_mesh;
  LDKAssetMaterial mountain_decoration_1_material;
  //@inspect min=0
  u32 mountain_decoration_1_min_count;
  //@inspect min=0
  u32 mountain_decoration_1_max_count;
  //@inspect min=0.01
  float mountain_decoration_1_min_scale;
  //@inspect min=0.01
  float mountain_decoration_1_max_scale;
  //@end_group

  //@begin_group "Slot 2"
  LDKAssetMesh mountain_decoration_2_mesh;
  LDKAssetMaterial mountain_decoration_2_material;
  //@inspect min=0
  u32 mountain_decoration_2_min_count;
  //@inspect min=0
  u32 mountain_decoration_2_max_count;
  //@inspect min=0.01
  float mountain_decoration_2_min_scale;
  //@inspect min=0.01
  float mountain_decoration_2_max_scale;
  //@end_group

  //@begin_group "Slot 3"
  LDKAssetMesh mountain_decoration_3_mesh;
  LDKAssetMaterial mountain_decoration_3_material;
  //@inspect min=0
  u32 mountain_decoration_3_min_count;
  //@inspect min=0
  u32 mountain_decoration_3_max_count;
  //@inspect min=0.01
  float mountain_decoration_3_min_scale;
  //@inspect min=0.01
  float mountain_decoration_3_max_scale;
  //@end_group

  //@begin_group "Slot 4"
  LDKAssetMesh mountain_decoration_4_mesh;
  LDKAssetMaterial mountain_decoration_4_material;
  //@inspect min=0
  u32 mountain_decoration_4_min_count;
  //@inspect min=0
  u32 mountain_decoration_4_max_count;
  //@inspect min=0.01
  float mountain_decoration_4_min_scale;
  //@inspect min=0.01
  float mountain_decoration_4_max_scale;
  //@end_group

  //@begin_group "Slot 5"
  LDKAssetMesh mountain_decoration_5_mesh;
  LDKAssetMaterial mountain_decoration_5_material;
  //@inspect min=0
  u32 mountain_decoration_5_min_count;
  //@inspect min=0
  u32 mountain_decoration_5_max_count;
  //@inspect min=0.01
  float mountain_decoration_5_min_scale;
  //@inspect min=0.01
  float mountain_decoration_5_max_scale;
  //@end_group

  //@begin_group "Slot 6"
  LDKAssetMesh mountain_decoration_6_mesh;
  LDKAssetMaterial mountain_decoration_6_material;
  //@inspect min=0
  u32 mountain_decoration_6_min_count;
  //@inspect min=0
  u32 mountain_decoration_6_max_count;
  //@inspect min=0.01
  float mountain_decoration_6_min_scale;
  //@inspect min=0.01
  float mountain_decoration_6_max_scale;
  //@end_group

  //@begin_group "Slot 7"
  LDKAssetMesh mountain_decoration_7_mesh;
  LDKAssetMaterial mountain_decoration_7_material;
  //@inspect min=0
  u32 mountain_decoration_7_min_count;
  //@inspect min=0
  u32 mountain_decoration_7_max_count;
  //@inspect min=0.01
  float mountain_decoration_7_min_scale;
  //@inspect min=0.01
  float mountain_decoration_7_max_scale;
  //@end_group

  //@end_group

  //@end_group

  //@begin_group "Runtime"
  //@inspect runtime
  i32 center_x;
  //@inspect runtime
  i32 center_y;
  //@inspect runtime
  u32 map_revision;
  //@inspect runtime
  u32 active_cell_count;
  //@inspect runtime
  u32 batch_count;
  //@inspect runtime
  u32 instance_count;
  //@inspect runtime
  u32 rebuild_count;
  //@end_group
} TFTFDecorationSystem;

#endif // TFTF_DECORATION_SYSTEM_H
