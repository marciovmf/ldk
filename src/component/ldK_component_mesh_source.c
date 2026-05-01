#include <ldk_asset_manager.h>
#include <stdx/stdx_hpool.h>
#include <ldk_component_mesh_source.h>

LDKMeshSource ldk_mesh_source_make_default(void)
{
  LDKMeshSource mesh_source = {0};

  mesh_source.source_asset = ldk_asset_mesh_null();
  mesh_source.version = 1;
  mesh_source.renderer_mesh = 0;
  mesh_source.renderer_version = 0;

  return mesh_source;
}

bool ldk_mesh_source_set_data(LDKMeshSource* mesh_source, LDKAssetMesh asset)
{
  if (!mesh_source)
    return false;

  if (x_handle_is_null(asset.h))
    return false;

  mesh_source->source_asset = asset;
  return true;
}
