#include <ldk_package.h>

#include <stdx/stdx_filesystem.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LDK_PACKAGE_VERSION 1u
#define LDK_PACKAGE_HEADER_SIZE 32u
#define LDK_PACKAGE_MANIFEST_ENTRY_SIZE 40u
#define LDK_PACKAGE_IO_BUFFER_SIZE (64u * 1024u)

static const u8 s_package_magic[8] = {'L', 'D', 'K', 'B', 'O', 'X', 0, 0};

typedef struct LDKPackageWriterEntry
{
  u64 path_hash;
  u64 data_offset;
  u64 stored_size;
  u64 size;
  u32 compression;
  char *path;
} LDKPackageWriterEntry;

struct LDKPackageEntry
{
  u64 path_hash;
  u64 data_offset;
  u64 stored_size;
  u64 size;
  u32 compression;
  const char *path;
};

struct LDKPackage
{
  FILE *file;
  LDKPackageEntry *entries;
  u8 *manifest_data;
  u32 entry_count;
};

struct LDKPackageWriter
{
  FILE *file;
  LDKPackageWriterEntry *entries;
  u32 entry_count;
  u32 entry_capacity;
  char *path;
  bool finalized;
  bool failed;
};

static char *s_string_copy(const char *text)
{
  size_t length;
  char *copy;

  if (text == NULL)
  {
    return NULL;
  }

  length = strlen(text);
  copy = (char *)malloc(length + 1);
  if (copy == NULL)
  {
    return NULL;
  }

  memcpy(copy, text, length + 1);
  return copy;
}

static u64 s_path_hash(const char *path)
{
  u64 hash = 14695981039346656037ull;

  while (*path)
  {
    hash ^= (u8)*path;
    hash *= 1099511628211ull;
    path += 1;
  }

  return hash;
}

static bool s_package_path_component_valid(
    const char *component, size_t length)
{
  if (length == 0)
  {
    return false;
  }

  if (length == 1 && component[0] == '.')
  {
    return false;
  }

  if (length == 2 && component[0] == '.' && component[1] == '.')
  {
    return false;
  }

  return true;
}

static bool s_package_path_valid(const char *path)
{
  size_t length;
  size_t component_start;

  if (path == NULL || path[0] == 0)
  {
    return false;
  }

  length = strlen(path);
  if (length > LDK_PACKAGE_PATH_MAX_LENGTH)
  {
    return false;
  }

  if (path[0] == '/' || path[length - 1] == '/')
  {
    return false;
  }

  component_start = 0;
  for (size_t i = 0; i <= length; ++i)
  {
    char c = path[i];

    if (c == '\\' || c == ':')
    {
      return false;
    }

    if (c == '/' || c == 0)
    {
      if (!s_package_path_component_valid(
              path + component_start, i - component_start))
      {
        return false;
      }
      component_start = i + 1;
    }
  }

  return true;
}

static void s_u32_write_le(u8 *out, u32 value)
{
  out[0] = (u8)(value & 0xffu);
  out[1] = (u8)((value >> 8u) & 0xffu);
  out[2] = (u8)((value >> 16u) & 0xffu);
  out[3] = (u8)((value >> 24u) & 0xffu);
}

static void s_u64_write_le(u8 *out, u64 value)
{
  for (u32 i = 0; i < 8; ++i)
  {
    out[i] = (u8)((value >> (i * 8u)) & 0xffu);
  }
}

static u32 s_u32_read_le(const u8 *data)
{
  return (u32)data[0] | ((u32)data[1] << 8u) | ((u32)data[2] << 16u) |
         ((u32)data[3] << 24u);
}

static u64 s_u64_read_le(const u8 *data)
{
  u64 value = 0;

  for (u32 i = 0; i < 8; ++i)
  {
    value |= ((u64)data[i]) << (i * 8u);
  }

  return value;
}

static bool s_file_seek(FILE *file, u64 offset)
{
#ifdef _WIN32
  if (offset > (u64)INT64_MAX)
  {
    return false;
  }
  return _fseeki64(file, (int64_t)offset, SEEK_SET) == 0;
#else
  if (offset > (u64)LONG_MAX)
  {
    return false;
  }
  return fseek(file, (long)offset, SEEK_SET) == 0;
#endif
}

static bool s_file_seek_end(FILE *file)
{
#ifdef _WIN32
  return _fseeki64(file, 0, SEEK_END) == 0;
#else
  return fseek(file, 0, SEEK_END) == 0;
#endif
}

static bool s_file_tell(FILE *file, u64 *out_offset)
{
#ifdef _WIN32
  int64_t position = _ftelli64(file);
#else
  long position = ftell(file);
#endif

  if (position < 0)
  {
    return false;
  }

  *out_offset = (u64)position;
  return true;
}

static bool s_file_size(FILE *file, u64 *out_size)
{
  u64 current;
  u64 size;

  if (!s_file_tell(file, &current))
  {
    return false;
  }

  if (!s_file_seek_end(file) || !s_file_tell(file, &size) ||
      !s_file_seek(file, current))
  {
    return false;
  }

  *out_size = size;
  return true;
}

static bool s_file_write_all(FILE *file, const void *data, u64 size)
{
  const u8 *cursor = (const u8 *)data;

  while (size > 0)
  {
    size_t chunk = size > LDK_PACKAGE_IO_BUFFER_SIZE
                       ? LDK_PACKAGE_IO_BUFFER_SIZE
                       : (size_t)size;
    if (fwrite(cursor, 1, chunk, file) != chunk)
    {
      return false;
    }
    cursor += chunk;
    size -= (u64)chunk;
  }

  return true;
}

static bool s_file_read_all(FILE *file, void *data, u64 size)
{
  u8 *cursor = (u8 *)data;

  while (size > 0)
  {
    size_t chunk = size > LDK_PACKAGE_IO_BUFFER_SIZE
                       ? LDK_PACKAGE_IO_BUFFER_SIZE
                       : (size_t)size;
    if (fread(cursor, 1, chunk, file) != chunk)
    {
      return false;
    }
    cursor += chunk;
    size -= (u64)chunk;
  }

  return true;
}

static bool s_file_copy(FILE *source, FILE *destination, u64 size)
{
  u8 buffer[LDK_PACKAGE_IO_BUFFER_SIZE];

  while (size > 0)
  {
    size_t chunk = size > sizeof(buffer) ? sizeof(buffer) : (size_t)size;
    if (fread(buffer, 1, chunk, source) != chunk)
    {
      return false;
    }
    if (fwrite(buffer, 1, chunk, destination) != chunk)
    {
      return false;
    }
    size -= (u64)chunk;
  }

  return true;
}

static bool s_header_write(FILE *file, u32 entry_count, u64 manifest_offset,
    u64 manifest_size)
{
  u8 header[LDK_PACKAGE_HEADER_SIZE] = {0};

  memcpy(header, s_package_magic, sizeof(s_package_magic));
  s_u32_write_le(header + 8, LDK_PACKAGE_VERSION);
  s_u32_write_le(header + 12, entry_count);
  s_u64_write_le(header + 16, manifest_offset);
  s_u64_write_le(header + 24, manifest_size);

  return s_file_write_all(file, header, sizeof(header));
}

static int s_entry_compare_values(
    u64 hash_a, const char *path_a, u64 hash_b, const char *path_b)
{
  if (hash_a < hash_b)
  {
    return -1;
  }
  if (hash_a > hash_b)
  {
    return 1;
  }
  return strcmp(path_a, path_b);
}

static int s_writer_entry_compare(const void *a, const void *b)
{
  const LDKPackageWriterEntry *entry_a = (const LDKPackageWriterEntry *)a;
  const LDKPackageWriterEntry *entry_b = (const LDKPackageWriterEntry *)b;

  return s_entry_compare_values(entry_a->path_hash, entry_a->path,
      entry_b->path_hash, entry_b->path);
}

static bool s_package_entry_owned(
    const LDKPackage *package, const LDKPackageEntry *entry)
{
  uintptr_t address;
  uintptr_t begin;
  uintptr_t end;

  if (package == NULL || entry == NULL || package->entries == NULL)
  {
    return false;
  }

  address = (uintptr_t)entry;
  begin = (uintptr_t)package->entries;
  end = begin + (uintptr_t)package->entry_count * sizeof(LDKPackageEntry);

  if (address < begin || address >= end)
  {
    return false;
  }

  return ((address - begin) % sizeof(LDKPackageEntry)) == 0;
}

static bool s_package_entry_data_valid(
    const LDKPackageEntry *entry, u64 manifest_offset)
{
  if (entry->data_offset < LDK_PACKAGE_HEADER_SIZE ||
      entry->data_offset > manifest_offset)
  {
    return false;
  }

  if (entry->stored_size > manifest_offset - entry->data_offset)
  {
    return false;
  }

  if (entry->compression == LDK_PACKAGE_COMPRESSION_NONE &&
      entry->stored_size != entry->size)
  {
    return false;
  }

  return true;
}

LDKPackage *ldk_package_open(const char *path)
{
  FILE *file = NULL;
  LDKPackage *package = NULL;
  LDKPackageEntry *entries = NULL;
  u8 *manifest_data = NULL;
  u8 header[LDK_PACKAGE_HEADER_SIZE];
  u64 file_size;
  u64 manifest_offset;
  u64 manifest_size;
  u32 version;
  u32 entry_count;
  size_t cursor = 0;

  if (path == NULL || path[0] == 0)
  {
    return NULL;
  }

  file = fopen(path, "rb");
  if (file == NULL)
  {
    return NULL;
  }

  if (!s_file_size(file, &file_size) || file_size < sizeof(header) ||
      !s_file_seek(file, 0) || !s_file_read_all(file, header, sizeof(header)))
  {
    fclose(file);
    return NULL;
  }

  if (memcmp(header, s_package_magic, sizeof(s_package_magic)) != 0)
  {
    fclose(file);
    return NULL;
  }

  version = s_u32_read_le(header + 8);
  entry_count = s_u32_read_le(header + 12);
  manifest_offset = s_u64_read_le(header + 16);
  manifest_size = s_u64_read_le(header + 24);

  if (version != LDK_PACKAGE_VERSION ||
      manifest_offset < LDK_PACKAGE_HEADER_SIZE ||
      manifest_offset > file_size ||
      manifest_size != file_size - manifest_offset)
  {
    fclose(file);
    return NULL;
  }

  if (entry_count == 0)
  {
    if (manifest_size != 0)
    {
      fclose(file);
      return NULL;
    }
  }
  else
  {
    const u64 minimum_entry_size = LDK_PACKAGE_MANIFEST_ENTRY_SIZE + 2u;
    if (manifest_size < minimum_entry_size ||
        (u64)entry_count > manifest_size / minimum_entry_size ||
        manifest_size > (u64)SIZE_MAX)
    {
      fclose(file);
      return NULL;
    }

    manifest_data = (u8 *)malloc((size_t)manifest_size);
    entries = (LDKPackageEntry *)calloc(entry_count, sizeof(LDKPackageEntry));
    if (manifest_data == NULL || entries == NULL)
    {
      free(entries);
      free(manifest_data);
      fclose(file);
      return NULL;
    }

    if (!s_file_seek(file, manifest_offset) ||
        !s_file_read_all(file, manifest_data, manifest_size))
    {
      free(entries);
      free(manifest_data);
      fclose(file);
      return NULL;
    }
  }

  for (u32 i = 0; i < entry_count; ++i)
  {
    LDKPackageEntry *entry = &entries[i];
    u32 path_length;
    size_t required;

    if ((u64)cursor + LDK_PACKAGE_MANIFEST_ENTRY_SIZE > manifest_size)
    {
      free(entries);
      free(manifest_data);
      fclose(file);
      return NULL;
    }

    entry->path_hash = s_u64_read_le(manifest_data + cursor);
    entry->data_offset = s_u64_read_le(manifest_data + cursor + 8);
    entry->stored_size = s_u64_read_le(manifest_data + cursor + 16);
    entry->size = s_u64_read_le(manifest_data + cursor + 24);
    entry->compression = s_u32_read_le(manifest_data + cursor + 32);
    path_length = s_u32_read_le(manifest_data + cursor + 36);
    cursor += LDK_PACKAGE_MANIFEST_ENTRY_SIZE;

    if (path_length == 0 || path_length > LDK_PACKAGE_PATH_MAX_LENGTH)
    {
      free(entries);
      free(manifest_data);
      fclose(file);
      return NULL;
    }

    required = (size_t)path_length + 1u;
    if ((u64)cursor + required > manifest_size)
    {
      free(entries);
      free(manifest_data);
      fclose(file);
      return NULL;
    }

    entry->path = (const char *)(manifest_data + cursor);
    if (entry->path[path_length] != 0 ||
        memchr(entry->path, 0, path_length) != NULL ||
        !s_package_path_valid(entry->path) ||
        entry->path_hash != s_path_hash(entry->path) ||
        !s_package_entry_data_valid(entry, manifest_offset))
    {
      free(entries);
      free(manifest_data);
      fclose(file);
      return NULL;
    }

    if (i > 0)
    {
      const LDKPackageEntry *previous = &entries[i - 1];
      int comparison = s_entry_compare_values(previous->path_hash,
          previous->path, entry->path_hash, entry->path);
      if (comparison >= 0)
      {
        free(entries);
        free(manifest_data);
        fclose(file);
        return NULL;
      }
    }

    cursor += required;
  }

  if ((u64)cursor != manifest_size)
  {
    free(entries);
    free(manifest_data);
    fclose(file);
    return NULL;
  }

  package = (LDKPackage *)calloc(1, sizeof(LDKPackage));
  if (package == NULL)
  {
    free(entries);
    free(manifest_data);
    fclose(file);
    return NULL;
  }

  package->file = file;
  package->entries = entries;
  package->manifest_data = manifest_data;
  package->entry_count = entry_count;
  return package;
}

void ldk_package_close(LDKPackage *package)
{
  if (package == NULL)
  {
    return;
  }

  if (package->file != NULL)
  {
    fclose(package->file);
  }

  free(package->entries);
  free(package->manifest_data);
  free(package);
}

u32 ldk_package_entry_count(const LDKPackage *package)
{
  return package != NULL ? package->entry_count : 0;
}

const LDKPackageEntry *ldk_package_entry_at(
    const LDKPackage *package, u32 index)
{
  if (package == NULL || index >= package->entry_count)
  {
    return NULL;
  }

  return &package->entries[index];
}

const LDKPackageEntry *ldk_package_entry_find(
    const LDKPackage *package, const char *path)
{
  u64 path_hash;
  u32 low;
  u32 high;

  if (package == NULL || !s_package_path_valid(path))
  {
    return NULL;
  }

  path_hash = s_path_hash(path);
  low = 0;
  high = package->entry_count;

  while (low < high)
  {
    u32 middle = low + (high - low) / 2u;
    const LDKPackageEntry *entry = &package->entries[middle];
    int comparison =
        s_entry_compare_values(path_hash, path, entry->path_hash, entry->path);

    if (comparison < 0)
    {
      high = middle;
    }
    else if (comparison > 0)
    {
      low = middle + 1u;
    }
    else
    {
      return entry;
    }
  }

  return NULL;
}

const char *ldk_package_entry_get_path(const LDKPackageEntry *entry)
{
  return entry != NULL ? entry->path : NULL;
}

u64 ldk_package_entry_get_size(const LDKPackageEntry *entry)
{
  return entry != NULL ? entry->size : 0;
}

u64 ldk_package_entry_get_stored_size(const LDKPackageEntry *entry)
{
  return entry != NULL ? entry->stored_size : 0;
}

LDKPackageCompression ldk_package_entry_get_compression(
    const LDKPackageEntry *entry)
{
  if (entry == NULL)
  {
    return LDK_PACKAGE_COMPRESSION_NONE;
  }

  return (LDKPackageCompression)entry->compression;
}

bool ldk_package_entry_read(LDKPackage *package, const LDKPackageEntry *entry,
    void *out_data, u64 out_size)
{
  if (!s_package_entry_owned(package, entry) || out_size < entry->size ||
      (entry->size > 0 && out_data == NULL))
  {
    return false;
  }

  switch (entry->compression)
  {
  case LDK_PACKAGE_COMPRESSION_NONE:
    if (entry->size == 0)
    {
      return true;
    }
    return s_file_seek(package->file, entry->data_offset) &&
           s_file_read_all(package->file, out_data, entry->size);
  default:
    return false;
  }
}

static bool s_destination_parent_create(const char *destination_path)
{
  XFSPath path;
  XFSPath parent;

  if (destination_path == NULL || destination_path[0] == 0 ||
      !x_fs_path_set(&path, destination_path))
  {
    return false;
  }

  x_fs_path_normalize(&path);
  if (x_fs_path_dirname(&path, &parent) == 0 || parent.length == 0)
  {
    return true;
  }

  return x_fs_directory_create_recursive(x_fs_path_cstr(&parent));
}

bool ldk_package_entry_extract(LDKPackage *package,
    const LDKPackageEntry *entry, const char *destination_path)
{
  FILE *destination;
  bool result = false;

  if (!s_package_entry_owned(package, entry) || destination_path == NULL ||
      destination_path[0] == 0 ||
      !s_destination_parent_create(destination_path))
  {
    return false;
  }

  destination = fopen(destination_path, "wb");
  if (destination == NULL)
  {
    return false;
  }

  switch (entry->compression)
  {
  case LDK_PACKAGE_COMPRESSION_NONE:
    result = s_file_seek(package->file, entry->data_offset) &&
             s_file_copy(package->file, destination, entry->stored_size);
    break;
  default:
    result = false;
    break;
  }

  if (fclose(destination) != 0)
  {
    result = false;
  }

  if (!result)
  {
    remove(destination_path);
  }

  return result;
}

bool ldk_package_extract_all(
    LDKPackage *package, const char *destination_directory)
{
  XFSPath destination_root;

  if (package == NULL || destination_directory == NULL ||
      destination_directory[0] == 0 ||
      !x_fs_path_set(&destination_root, destination_directory))
  {
    return false;
  }

  x_fs_path_normalize(&destination_root);
  if (!x_fs_directory_create_recursive(x_fs_path_cstr(&destination_root)))
  {
    return false;
  }

  for (u32 i = 0; i < package->entry_count; ++i)
  {
    XFSPath destination = destination_root;

    if (x_fs_path_join(&destination, package->entries[i].path) == 0 ||
        !ldk_package_entry_extract(
            package, &package->entries[i], x_fs_path_cstr(&destination)))
    {
      return false;
    }
  }

  return true;
}

static bool s_writer_entries_reserve(
    LDKPackageWriter *writer, u32 required_capacity)
{
  LDKPackageWriterEntry *entries;
  u32 capacity;

  if (required_capacity <= writer->entry_capacity)
  {
    return true;
  }

  capacity = writer->entry_capacity > 0 ? writer->entry_capacity : 16u;
  while (capacity < required_capacity)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required_capacity;
      break;
    }
    capacity *= 2u;
  }

  size_t allocation_size =
      (size_t)capacity * sizeof(LDKPackageWriterEntry);
  if (capacity != 0 &&
      allocation_size / sizeof(LDKPackageWriterEntry) != (size_t)capacity)
  {
    return false;
  }

  entries = (LDKPackageWriterEntry *)realloc(
      writer->entries, allocation_size);
  if (entries == NULL)
  {
    return false;
  }

  writer->entries = entries;
  writer->entry_capacity = capacity;
  return true;
}

LDKPackageWriter *ldk_package_writer_create(const char *path)
{
  LDKPackageWriter *writer;

  if (path == NULL || path[0] == 0)
  {
    return NULL;
  }

  writer = (LDKPackageWriter *)calloc(1, sizeof(LDKPackageWriter));
  if (writer == NULL)
  {
    return NULL;
  }

  writer->path = s_string_copy(path);
  if (writer->path == NULL)
  {
    free(writer);
    return NULL;
  }

  writer->file = fopen(path, "wb+");
  if (writer->file == NULL)
  {
    free(writer->path);
    free(writer);
    return NULL;
  }

  if (!s_header_write(writer->file, 0, LDK_PACKAGE_HEADER_SIZE, 0))
  {
    fclose(writer->file);
    remove(path);
    free(writer->path);
    free(writer);
    return NULL;
  }

  return writer;
}

bool ldk_package_writer_add_file(LDKPackageWriter *writer,
    const char *source_path, const char *package_path,
    LDKPackageCompression compression)
{
  FILE *source;
  u64 source_size;
  u64 data_offset;
  char *path_copy;
  LDKPackageWriterEntry *entry;

  if (writer == NULL || writer->file == NULL || writer->finalized ||
      writer->failed || source_path == NULL || source_path[0] == 0 ||
      !s_package_path_valid(package_path) ||
      compression != LDK_PACKAGE_COMPRESSION_NONE ||
      writer->entry_count == UINT32_MAX)
  {
    return false;
  }

  path_copy = s_string_copy(package_path);
  if (path_copy == NULL ||
      !s_writer_entries_reserve(writer, writer->entry_count + 1u))
  {
    free(path_copy);
    return false;
  }

  source = fopen(source_path, "rb");
  if (source == NULL)
  {
    free(path_copy);
    return false;
  }

  if (!s_file_size(source, &source_size) || !s_file_seek(source, 0) ||
      !s_file_tell(writer->file, &data_offset))
  {
    fclose(source);
    free(path_copy);
    return false;
  }

  if (!s_file_copy(source, writer->file, source_size))
  {
    fclose(source);
    free(path_copy);
    writer->failed = true;
    return false;
  }

  fclose(source);

  entry = &writer->entries[writer->entry_count++];
  memset(entry, 0, sizeof(*entry));
  entry->path_hash = s_path_hash(package_path);
  entry->data_offset = data_offset;
  entry->stored_size = source_size;
  entry->size = source_size;
  entry->compression = (u32)compression;
  entry->path = path_copy;
  return true;
}

static bool s_writer_manifest_write(
    LDKPackageWriter *writer, u64 *out_offset, u64 *out_size)
{
  u64 manifest_offset;
  u64 manifest_end;

  if (!s_file_tell(writer->file, &manifest_offset))
  {
    return false;
  }

  for (u32 i = 0; i < writer->entry_count; ++i)
  {
    const LDKPackageWriterEntry *entry = &writer->entries[i];
    size_t path_length = strlen(entry->path);
    u8 data[LDK_PACKAGE_MANIFEST_ENTRY_SIZE] = {0};

    s_u64_write_le(data, entry->path_hash);
    s_u64_write_le(data + 8, entry->data_offset);
    s_u64_write_le(data + 16, entry->stored_size);
    s_u64_write_le(data + 24, entry->size);
    s_u32_write_le(data + 32, entry->compression);
    s_u32_write_le(data + 36, (u32)path_length);

    if (!s_file_write_all(writer->file, data, sizeof(data)) ||
        !s_file_write_all(writer->file, entry->path, (u64)path_length + 1u))
    {
      return false;
    }
  }

  if (!s_file_tell(writer->file, &manifest_end))
  {
    return false;
  }

  *out_offset = manifest_offset;
  *out_size = manifest_end - manifest_offset;
  return true;
}

bool ldk_package_writer_finalize(LDKPackageWriter *writer)
{
  u64 manifest_offset;
  u64 manifest_size;
  bool result;

  if (writer == NULL || writer->file == NULL || writer->finalized ||
      writer->failed)
  {
    return false;
  }

  if (writer->entry_count > 1)
  {
    qsort(writer->entries, writer->entry_count, sizeof(LDKPackageWriterEntry),
        s_writer_entry_compare);

    for (u32 i = 1; i < writer->entry_count; ++i)
    {
      if (strcmp(writer->entries[i - 1].path, writer->entries[i].path) == 0)
      {
        writer->failed = true;
        return false;
      }
    }
  }

  result = s_writer_manifest_write(writer, &manifest_offset, &manifest_size) &&
           s_file_seek(writer->file, 0) &&
           s_header_write(writer->file, writer->entry_count, manifest_offset,
               manifest_size) &&
           fflush(writer->file) == 0;

  if (fclose(writer->file) != 0)
  {
    result = false;
  }
  writer->file = NULL;

  if (!result)
  {
    writer->failed = true;
    remove(writer->path);
    return false;
  }

  writer->finalized = true;
  return true;
}

void ldk_package_writer_destroy(LDKPackageWriter *writer)
{
  if (writer == NULL)
  {
    return;
  }

  if (writer->file != NULL)
  {
    fclose(writer->file);
  }

  if (!writer->finalized && writer->path != NULL)
  {
    remove(writer->path);
  }

  for (u32 i = 0; i < writer->entry_count; ++i)
  {
    free(writer->entries[i].path);
  }

  free(writer->entries);
  free(writer->path);
  free(writer);
}
