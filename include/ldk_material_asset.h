#ifndef LDK_MATERIAL_ASSET_H
#define LDK_MATERIAL_ASSET_H

#include <ldk_material_io.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct LDKAssetMaterialData
  {
    LDKMaterialDesc descriptor;
    u64 revision;
    bool dirty;
    bool is_missing; /* Read-only procedural fallback; preserves the file path. */
  } LDKAssetMaterialData;

  /* Material assets are manager-owned until clear/termination. Image references
   * are shared. Use update instead of mutating the descriptor directly.
   * Paths may be absolute or runtree-relative; identity is the normalized
   * absolute path within this manager. No automatic file reload is performed. */
  LDK_API LDKAssetMaterial ldk_asset_material_null(void);
  LDK_API const LDKAssetMaterialData *ldk_asset_manager_material_get_const(
      LDKAssetManager *manager, LDKAssetMaterial asset);

  /* Create an unsaved asset (revision 1, dirty). Fails if the path is already
   * cached or exists on disk. Does not write a file or create directories. */
  LDK_API LDKAssetMaterial ldk_asset_manager_material_create(
      const LDKMaterialIOContext *context, const char *path,
      const LDKMaterialDesc *descriptor, LDKMaterialIOResult *result);

  /* Reuses an existing cached asset, including unsaved edits. Otherwise reads
   * a material: node containing the existing material_type/color/texture fields.
   * Texture references remain relative to the project runtree. Missing files
   * return a cached unlit magenta-checker placeholder and emit an error diagnostic.
   * Invalid files and allocation failures still return a null handle. */
  LDK_API LDKAssetMaterial ldk_asset_manager_material_load_shared(
      const LDKMaterialIOContext *context, const char *path,
      LDKMaterialIOResult *result);

  /* Equal descriptors are a successful no-op. Changes increment revision and
   * set dirty. Invalid handles/descriptors leave the asset untouched. */
  LDK_API bool ldk_asset_manager_material_update(LDKAssetManager *manager,
      LDKAssetMaterial asset, const LDKMaterialDesc *descriptor);

  /* Saves to the asset's own path. Clears dirty only after a successful write
   * and close; saving does not change revision. result is optional. */
  LDK_API bool ldk_asset_manager_material_save(
      const LDKMaterialIOContext *context, LDKAssetMaterial asset,
      LDKMaterialIOResult *result);

#ifdef __cplusplus
}
#endif

#endif /* LDK_MATERIAL_ASSET_H */
