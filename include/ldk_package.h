/**
 * # BOX packages
 *
 * Read and write uncompressed BOX asset packages, inspect entries, and extract stored files.
 */

#ifndef LDK_PACKAGE_H
#define LDK_PACKAGE_H

#include <ldk_common.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @group ldk.package */
#define LDK_PACKAGE_FILE_EXTENSION ".box"
/** @group ldk.package */
#define LDK_PACKAGE_PATH_MAX_LENGTH 511u

  /** @group ldk.package */
  typedef struct LDKPackage LDKPackage;
  /** @group ldk.package */
  typedef struct LDKPackageEntry LDKPackageEntry;
  /** @group ldk.package */
  typedef struct LDKPackageWriter LDKPackageWriter;

  /** @group ldk.package */
  typedef enum LDKPackageCompression
  {
    LDK_PACKAGE_COMPRESSION_NONE = 0
  } LDKPackageCompression;

  /* Returns true when path is a valid logical path inside a package. */
  /**
   * @brief Validate a logical path within a BOX package.
   * @param path Path identifying the file or resource.
   * @return True when the queried condition holds; false otherwise.
   * @group ldk.package
   */
  LDK_API bool ldk_package_path_is_valid(const char *path);

  /* Computes the FNV-1a 64-bit hash of the complete package file. */
  /**
   * @brief Compute the FNV-1a hash of a package file.
   * @param path Path identifying the file or resource.
   * @param out_hash Receives the computed hash.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.package
   */
  LDK_API bool ldk_package_file_hash(const char *path, u64 *out_hash);

  /* Opens a package and loads its manifest. The package keeps its file handle
   * open until ldk_package_close(). */
  /**
   * @brief Open a BOX package and read its manifest.
   *
   * The reader keeps the package file open until ldk_package_close() is called.
   * @param path Path identifying the file or resource.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.package
   */
  LDK_API LDKPackage *ldk_package_open(const char *path);
  /**
   * @brief Close an open package and release its reader resources.
   * @param package Opened package used by this operation.
   * @group ldk.package
   */
  LDK_API void ldk_package_close(LDKPackage *package);

  /**
   * @brief Count the package entry.
   * @param package Opened package used by this operation.
   * @return Number of matching entries.
   * @group ldk.package
   */
  LDK_API u32 ldk_package_entry_count(const LDKPackage *package);
  /**
   * @brief Get the package entry by index.
   * @param package Opened package used by this operation.
   * @param index Zero-based index of the selected item.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.package
   */
  LDK_API const LDKPackageEntry *ldk_package_entry_at(
      const LDKPackage *package, u32 index);
  /**
   * @brief Find the package entry.
   * @param package Opened package used by this operation.
   * @param path Path identifying the file or resource.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.package
   */
  LDK_API const LDKPackageEntry *ldk_package_entry_find(
      const LDKPackage *package, const char *path);

  /**
   * @brief Get the logical path of a package entry.
   * @param entry Package entry being inspected or read.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.package
   */
  LDK_API const char *ldk_package_entry_get_path(
      const LDKPackageEntry *entry);
  /**
   * @brief Get the uncompressed byte size of an entry.
   * @param entry Package entry being inspected or read.
   * @return Size of the result, in bytes where applicable.
   * @group ldk.package
   */
  LDK_API u64 ldk_package_entry_get_size(const LDKPackageEntry *entry);
  /**
   * @brief Get the stored byte size of an entry.
   * @param entry Package entry being inspected or read.
   * @return Size of the result, in bytes where applicable.
   * @group ldk.package
   */
  LDK_API u64 ldk_package_entry_get_stored_size(
      const LDKPackageEntry *entry);
  /**
   * @brief Get the compression mode of a package entry.
   * @param entry Package entry being inspected or read.
   * @return The requested value.
   * @group ldk.package
   */
  LDK_API LDKPackageCompression ldk_package_entry_get_compression(
      const LDKPackageEntry *entry);

  /* Reads up to size bytes from an uncompressed entry at offset. Reaching the
   * end of the entry is not an error; out_read receives the bytes read. */
  /**
   * @brief Read a byte range from an uncompressed package entry.
   *
   * Reaching the end of an entry is not an error. The out_read parameter reports the number of bytes copied.
   * @param package Opened package used by this operation.
   * @param entry Package entry being inspected or read.
   * @param offset Byte offset into the resource or entry.
   * @param out_data Destination buffer for the resulting data.
   * @param size Size of the data or region, in bytes where applicable.
   * @param out_read Receives the number of bytes actually read.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.package
   */
  LDK_API bool ldk_package_entry_read_at(const LDKPackage *package,
      const LDKPackageEntry *entry, u64 offset, void *out_data, u64 size,
      u64 *out_read);

  /* Reads the complete uncompressed entry into caller-owned memory.
   * out_size must be at least ldk_package_entry_get_size(entry). */
  /**
   * @brief Copy an entire uncompressed package entry into caller-provided memory.
   *
   * The destination must have at least ldk_package_entry_get_size(entry) bytes available.
   * @param package Opened package used by this operation.
   * @param entry Package entry being inspected or read.
   * @param out_data Destination buffer for the resulting data.
   * @param out_size Receives or specifies the byte size of the output buffer, as defined by the operation.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.package
   */
  LDK_API bool ldk_package_entry_read(LDKPackage *package,
      const LDKPackageEntry *entry, void *out_data, u64 out_size);

  /* destination_path is the complete output filename. */
  /**
   * @brief Extract one package entry to a filesystem path.
   * @param package Opened package used by this operation.
   * @param entry Package entry being inspected or read.
   * @param destination_path Filesystem path for the extracted file.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.package
   */
  LDK_API bool ldk_package_entry_extract(LDKPackage *package,
      const LDKPackageEntry *entry, const char *destination_path);

  /* Recreates all package paths below destination_directory. */
  /**
   * @brief Extract every package entry below the requested destination directory.
   * @param package Opened package used by this operation.
   * @param destination_directory Filesystem directory where entries are extracted.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.package
   */
  LDK_API bool ldk_package_extract_all(
      LDKPackage *package, const char *destination_directory);

  /* Creates a package writer. The output file is removed if the writer is
   * destroyed before a successful finalize. */
  /**
   * @brief Create the package writer.
   *
   * An unfinished output file is removed if the writer is destroyed before finalization.
   * @param path Path identifying the file or resource.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.package
   */
  LDK_API LDKPackageWriter *ldk_package_writer_create(const char *path);
  /**
   * @brief Add an existing filesystem file to a package being written.
   * @param writer Package writer being updated.
   * @param source_path Filesystem path of the source file.
   * @param package_path Logical path of the file inside the package.
   * @param compression Compression mode stored for this package entry.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.package
   */
  LDK_API bool ldk_package_writer_add_file(LDKPackageWriter *writer,
      const char *source_path, const char *package_path,
      LDKPackageCompression compression);
  /**
   * @brief Finish writing the package and its manifest.
   * @param writer Package writer being updated.
   * @return True if the operation succeeds; false otherwise.
   * @group ldk.package
   */
  LDK_API bool ldk_package_writer_finalize(LDKPackageWriter *writer);
  /**
   * @brief Destroy the package writer.
   * @param writer Package writer being updated.
   * @group ldk.package
   */
  LDK_API void ldk_package_writer_destroy(LDKPackageWriter *writer);

#ifdef __cplusplus
}
#endif

#endif /* LDK_PACKAGE_H */
