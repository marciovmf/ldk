#ifndef LDK_PACKAGE_H
#define LDK_PACKAGE_H

#include <ldk_common.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LDK_PACKAGE_FILE_EXTENSION ".box"
#define LDK_PACKAGE_PATH_MAX_LENGTH 511u

  typedef struct LDKPackage LDKPackage;
  typedef struct LDKPackageEntry LDKPackageEntry;
  typedef struct LDKPackageWriter LDKPackageWriter;

  typedef enum LDKPackageCompression
  {
    LDK_PACKAGE_COMPRESSION_NONE = 0
  } LDKPackageCompression;

  /* Returns true when path is a valid logical path inside a package. */
  LDK_API bool ldk_package_path_is_valid(const char *path);

  /* Computes the FNV-1a 64-bit hash of the complete package file. */
  LDK_API bool ldk_package_file_hash(const char *path, u64 *out_hash);

  /* Opens a package and loads its manifest. The package keeps its file handle
   * open until ldk_package_close(). */
  LDK_API LDKPackage *ldk_package_open(const char *path);
  LDK_API void ldk_package_close(LDKPackage *package);

  LDK_API u32 ldk_package_entry_count(const LDKPackage *package);
  LDK_API const LDKPackageEntry *ldk_package_entry_at(
      const LDKPackage *package, u32 index);
  LDK_API const LDKPackageEntry *ldk_package_entry_find(
      const LDKPackage *package, const char *path);

  LDK_API const char *ldk_package_entry_get_path(
      const LDKPackageEntry *entry);
  LDK_API u64 ldk_package_entry_get_size(const LDKPackageEntry *entry);
  LDK_API u64 ldk_package_entry_get_stored_size(
      const LDKPackageEntry *entry);
  LDK_API LDKPackageCompression ldk_package_entry_get_compression(
      const LDKPackageEntry *entry);

  /* Reads up to size bytes from an uncompressed entry at offset. Reaching the
   * end of the entry is not an error; out_read receives the bytes read. */
  LDK_API bool ldk_package_entry_read_at(const LDKPackage *package,
      const LDKPackageEntry *entry, u64 offset, void *out_data, u64 size,
      u64 *out_read);

  /* Reads the complete uncompressed entry into caller-owned memory.
   * out_size must be at least ldk_package_entry_get_size(entry). */
  LDK_API bool ldk_package_entry_read(LDKPackage *package,
      const LDKPackageEntry *entry, void *out_data, u64 out_size);

  /* destination_path is the complete output filename. */
  LDK_API bool ldk_package_entry_extract(LDKPackage *package,
      const LDKPackageEntry *entry, const char *destination_path);

  /* Recreates all package paths below destination_directory. */
  LDK_API bool ldk_package_extract_all(
      LDKPackage *package, const char *destination_directory);

  /* Creates a package writer. The output file is removed if the writer is
   * destroyed before a successful finalize. */
  LDK_API LDKPackageWriter *ldk_package_writer_create(const char *path);
  LDK_API bool ldk_package_writer_add_file(LDKPackageWriter *writer,
      const char *source_path, const char *package_path,
      LDKPackageCompression compression);
  LDK_API bool ldk_package_writer_finalize(LDKPackageWriter *writer);
  LDK_API void ldk_package_writer_destroy(LDKPackageWriter *writer);

#ifdef __cplusplus
}
#endif

#endif /* LDK_PACKAGE_H */
