/**
 * @file   ldk_asset_source.h
 * @brief  Runtime asset byte source
 *
 * Resolves logical asset paths against open .box packages first and the
 * configured runtree directory second. It does not provide virtual filesystem
 * semantics; it only finds and reads complete files.
 */

#ifndef LDK_ASSET_SOURCE_H
#define LDK_ASSET_SOURCE_H

#include <ldk_common.h>
#include <ldk_package.h>
#include <stdx/stdx_filesystem.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct LDKAssetSourcePackage LDKAssetSourcePackage;

  typedef enum LDKAssetSourceOrigin
  {
    LDK_ASSET_SOURCE_ORIGIN_NONE = 0,
    LDK_ASSET_SOURCE_ORIGIN_PACKAGE,
    LDK_ASSET_SOURCE_ORIGIN_FILESYSTEM,
  } LDKAssetSourceOrigin;

  typedef struct LDKAssetSourceFile
  {
    LDKAssetSourceOrigin origin;
    u64 size;
    union
    {
      struct
      {
        LDKPackage *package;
        const LDKPackageEntry *entry;
      } package;
      XFSPath filesystem_path;
    } location;
  } LDKAssetSourceFile;

  typedef struct LDKAssetSource
  {
    XFSPath runtree_path;
    LDKAssetSourcePackage *packages;
  } LDKAssetSource;

  LDK_API bool ldk_asset_source_initialize(
      LDKAssetSource *source, const char *runtree_path);
  LDK_API void ldk_asset_source_terminate(LDKAssetSource *source);

  /* Opens a package and prepends it to the package lookup list. Relative
   * package paths are resolved below the configured runtree path. */
  LDK_API LDKAssetSourcePackage *ldk_asset_source_package_open(
      LDKAssetSource *source, const char *path);
  LDK_API bool ldk_asset_source_package_close(
      LDKAssetSource *source, LDKAssetSourcePackage *package);

  /* Resolves path using the fixed policy:
   *   1. first matching open package
   *   2. runtree/path on the local filesystem
   *   3. failure
   *
   * Logical paths use the same relative '/' path rules as package entries.
   * A package-backed result remains valid until that package is closed. */
  LDK_API bool ldk_asset_source_find(const LDKAssetSource *source,
      const char *path, LDKAssetSourceFile *out_file);

  LDK_API u64 ldk_asset_source_file_size(const LDKAssetSourceFile *file);

  /* Reads the complete file into caller-owned memory. out_size must be at
   * least ldk_asset_source_file_size(file). */
  LDK_API bool ldk_asset_source_file_read(
      const LDKAssetSourceFile *file, void *out_data, u64 out_size);

#ifdef __cplusplus
}
#endif

#endif /* LDK_ASSET_SOURCE_H */
