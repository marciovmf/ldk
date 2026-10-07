#include <module/ldk_asset_source.h>

#include <ldk.h>

#include <stdx/stdx_io.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#ifndef _WIN32
#include <sys/types.h>
#endif
#include <stdlib.h>
#include <string.h>

struct LDKAssetSourcePackage
{
  LDKPackage *package;
  XFSPath path;
  LDKAssetSourcePackage *next;
};

static void s_asset_source_revision_increment(LDKAssetSource *source)
{
  source->revision = source->revision == UINT64_MAX ? 1 : source->revision + 1;
}

bool ldk_asset_path_set(LDKAssetPath *out_path, const char *path)
{
  if (!out_path || !ldk_package_path_is_valid(path))
  {
    return false;
  }

  size_t length = strlen(path);
  memcpy(out_path->buf, path, length + 1);
  out_path->length = length;
  return true;
}

bool ldk_asset_source_initialize(
    LDKAssetSource *source, const char *runtree_path)
{
  if (!source || !runtree_path || !runtree_path[0])
  {
    return false;
  }

  memset(source, 0, sizeof(*source));
  return ldk_asset_source_runtree_set(source, runtree_path);
}

void ldk_asset_source_terminate(LDKAssetSource *source)
{
  if (!source)
  {
    return;
  }

  LDKAssetSourcePackage *node = source->packages;
  while (node)
  {
    LDKAssetSourcePackage *next = node->next;
    ldk_package_close(node->package);
    free(node);
    node = next;
  }

  memset(source, 0, sizeof(*source));
}

bool ldk_asset_source_runtree_set(
    LDKAssetSource *source, const char *runtree_path)
{
  XFSPath path;

  if (!source || !runtree_path || !runtree_path[0] ||
      !x_fs_path_set(&path, runtree_path))
  {
    return false;
  }

  x_fs_path_normalize(&path);
  source->runtree_path = path;
  s_asset_source_revision_increment(source);
  return true;
}

LDKAssetSourcePackage *ldk_asset_source_package_open(
    LDKAssetSource *source, const char *path)
{
  XFSPath physical_path;
  LDKPackage *package;
  LDKAssetSourcePackage *node;

  if (!source || !path || !path[0] ||
      !x_fs_path_set(&physical_path, path))
  {
    return NULL;
  }

  x_fs_path_normalize(&physical_path);
  package = ldk_package_open(x_fs_path_cstr(&physical_path));
  if (!package)
  {
    return NULL;
  }

  node = (LDKAssetSourcePackage *)malloc(sizeof(*node));
  if (!node)
  {
    ldk_package_close(package);
    return NULL;
  }

  node->package = package;
  node->path = physical_path;
  node->next = source->packages;
  source->packages = node;
  s_asset_source_revision_increment(source);
  return node;
}

bool ldk_asset_source_package_close(
    LDKAssetSource *source, LDKAssetSourcePackage *package)
{
  LDKAssetSourcePackage **link;

  if (!source || !package)
  {
    return false;
  }

  link = &source->packages;
  while (*link)
  {
    if (*link == package)
    {
      LDKAssetSourcePackage *node = *link;
      *link = node->next;
      ldk_package_close(node->package);
      free(node);
      s_asset_source_revision_increment(source);
      return true;
    }
    link = &(*link)->next;
  }

  return false;
}

const XFSPath *ldk_asset_source_package_path_get(
    const LDKAssetSource *source, const LDKPackage *package)
{
  if (!source || !package) return NULL;
  for (LDKAssetSourcePackage *node = source->packages; node; node = node->next)
  {
    if (node->package == package) return &node->path;
  }
  return NULL;
}

bool ldk_asset_source_find(const LDKAssetSource *source, const char *path,
    LDKAssetSourceFile *out_file)
{
  LDKAssetPath asset_path;
  LDKAssetSourcePackage *node;
  const LDKPackageEntry *entry;
  XFSPath filesystem_path;
  FSFileStat stat;

  if (!source || !out_file || !ldk_asset_path_set(&asset_path, path))
  {
    return false;
  }

  memset(out_file, 0, sizeof(*out_file));

  node = source->packages;
  while (node)
  {
    entry = ldk_package_entry_find(node->package, asset_path.buf);
    if (entry)
    {
      out_file->origin = LDK_ASSET_SOURCE_ORIGIN_PACKAGE;
      out_file->size = ldk_package_entry_get_size(entry);
      out_file->location.package.package = node->package;
      out_file->location.package.entry = entry;
      return true;
    }
    node = node->next;
  }

  if (!x_fs_path(&filesystem_path,
          x_fs_path_cstr(&source->runtree_path), asset_path.buf))
  {
    return false;
  }

  x_fs_path_normalize(&filesystem_path);
  if (!x_fs_path_is_file(&filesystem_path) ||
      !x_fs_file_stat(x_fs_path_cstr(&filesystem_path), &stat))
  {
    return false;
  }

  out_file->origin = LDK_ASSET_SOURCE_ORIGIN_FILESYSTEM;
  out_file->size = (u64)stat.size;
  out_file->location.filesystem_path = filesystem_path;
  return true;
}

u64 ldk_asset_source_file_size(const LDKAssetSourceFile *file)
{
  return file ? file->size : 0;
}

typedef struct LDKAssetSourceAsyncReadContext
{
  LDKAssetSourceFile file;
  u64 offset;
  void *out_data;
  u64 size;
  u64 *out_read;
} LDKAssetSourceAsyncReadContext;

static bool s_filesystem_seek(FILE *file, u64 offset)
{
#ifdef _WIN32
  return offset <= (u64)INT64_MAX &&
         _fseeki64(file, (int64_t)offset, SEEK_SET) == 0;
#else
  return offset <= (u64)INT64_MAX &&
         fseeko(file, (off_t)offset, SEEK_SET) == 0;
#endif
}

bool ldk_asset_source_file_read_at(const LDKAssetSourceFile *file,
    u64 offset, void *out_data, u64 size, u64 *out_read)
{
  u64 read_size;

  if (out_read)
  {
    *out_read = 0;
  }

  if (!file || file->origin == LDK_ASSET_SOURCE_ORIGIN_NONE ||
      offset > file->size)
  {
    return false;
  }

  read_size = file->size - offset;
  if (read_size > size)
  {
    read_size = size;
  }

  if (read_size > 0 && !out_data)
  {
    return false;
  }

  if (file->origin == LDK_ASSET_SOURCE_ORIGIN_PACKAGE)
  {
    return ldk_package_entry_read_at(file->location.package.package,
        file->location.package.entry, offset, out_data, read_size, out_read);
  }

  if (file->origin == LDK_ASSET_SOURCE_ORIGIN_FILESYSTEM)
  {
    FILE *input = fopen(
        x_fs_path_cstr(&file->location.filesystem_path), "rb");
    u8 *cursor = (u8 *)out_data;
    u64 total = 0;

    if (!input)
    {
      return false;
    }

    if (read_size > 0 && !s_filesystem_seek(input, offset))
    {
      fclose(input);
      return false;
    }

    while (total < read_size)
    {
      u64 remaining = read_size - total;
      size_t chunk = remaining > (u64)SIZE_MAX ? SIZE_MAX : (size_t)remaining;
      size_t count = fread(cursor, 1, chunk, input);
      cursor += count;
      total += (u64)count;
      if (count < chunk)
      {
        if (ferror(input))
        {
          fclose(input);
          return false;
        }
        break;
      }
    }

    fclose(input);
    if (out_read)
    {
      *out_read = total;
    }
    return true;
  }

  return false;
}

static bool s_asset_source_file_read_async(void *user_data)
{
  LDKAssetSourceAsyncReadContext *context =
      (LDKAssetSourceAsyncReadContext *)user_data;
  bool result = ldk_asset_source_file_read_at(&context->file,
      context->offset, context->out_data, context->size, context->out_read);
  free(context);
  return result;
}

LDKAsyncRead ldk_asset_source_file_read_at_async(
    const LDKAssetSourceFile *file, u64 offset, void *out_data, u64 size,
    u64 *out_read)
{
  LDKAsyncRead result = {0};
  LDKAssetSourceAsyncReadContext *context;
  LDKJobs *jobs;

  if (out_read)
  {
    *out_read = 0;
  }

  if (!file || file->origin == LDK_ASSET_SOURCE_ORIGIN_NONE ||
      offset > file->size ||
      (size > 0 && offset < file->size && !out_data))
  {
    return result;
  }

  jobs = (LDKJobs *)ldk_module_get(LDK_MODULE_JOBS);
  if (!jobs)
  {
    return result;
  }

  context = (LDKAssetSourceAsyncReadContext *)malloc(sizeof(*context));
  if (!context)
  {
    return result;
  }

  context->file = *file;
  context->offset = offset;
  context->out_data = out_data;
  context->size = size;
  context->out_read = out_read;

  result = ldk_jobs_submit(jobs, s_asset_source_file_read_async, context);
  if (!ldk_async_result_is_valid(result))
  {
    free(context);
  }

  return result;
}

LDKAsyncRead ldk_asset_source_file_read_async(
    const LDKAssetSourceFile *file, void *out_data, u64 out_size)
{
  LDKAsyncRead result = {0};

  if (!file || out_size < file->size ||
      (file->size > 0 && !out_data))
  {
    return result;
  }

  return ldk_asset_source_file_read_at_async(
      file, 0, out_data, file->size, NULL);
}

bool ldk_asset_source_file_read(
    const LDKAssetSourceFile *file, void *out_data, u64 out_size)
{
  u64 read_size = 0;

  if (!file || out_size < file->size ||
      (file->size > 0 && !out_data))
  {
    return false;
  }

  return ldk_asset_source_file_read_at(
             file, 0, out_data, file->size, &read_size) &&
         read_size == file->size;
}

bool ldk_asset_source_file_write(const LDKAssetSource *source,
    const char *path, const void *data, u64 size)
{
  LDKAssetPath asset_path;
  XFSPath filesystem_path;
  XFile *output;

  if (!source || !ldk_asset_path_set(&asset_path, path) ||
      size > (u64)SIZE_MAX || (size > 0 && !data) ||
      !x_fs_path(&filesystem_path,
          x_fs_path_cstr(&source->runtree_path), asset_path.buf))
  {
    return false;
  }

  x_fs_path_normalize(&filesystem_path);
  output = x_io_open(x_fs_path_cstr(&filesystem_path), "wb");
  if (!output)
  {
    return false;
  }

  bool ok = x_io_write(output, data, (size_t)size) == (size_t)size;
  x_io_close(output);
  return ok;
}
