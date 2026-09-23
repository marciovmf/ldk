#include <module/ldk_asset_source.h>

#include <stdx/stdx_io.h>

#include <stdint.h>
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

bool ldk_asset_source_file_read(
    const LDKAssetSourceFile *file, void *out_data, u64 out_size)
{
  if (!file || file->origin == LDK_ASSET_SOURCE_ORIGIN_NONE ||
      out_size < file->size || (file->size > 0 && !out_data))
  {
    return false;
  }

  if (file->origin == LDK_ASSET_SOURCE_ORIGIN_PACKAGE)
  {
    return ldk_package_entry_read(file->location.package.package,
        file->location.package.entry, out_data, out_size);
  }

  if (file->origin != LDK_ASSET_SOURCE_ORIGIN_FILESYSTEM ||
      file->size > (u64)SIZE_MAX)
  {
    return false;
  }

  XFile *input =
      x_io_open(x_fs_path_cstr(&file->location.filesystem_path), "rb");
  if (!input)
  {
    return false;
  }

  size_t offset = 0;
  size_t size = (size_t)file->size;
  while (offset < size)
  {
    size_t read = x_io_read(input, (u8 *)out_data + offset, size - offset);
    if (read == 0)
    {
      x_io_close(input);
      return false;
    }
    offset += read;
  }

  x_io_close(input);
  return true;
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
