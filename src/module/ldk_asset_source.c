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

static bool s_asset_source_physical_path(const LDKAssetSource *source,
    const char *path, XFSPath *out_path)
{
  if (!source || !path || !path[0] || !out_path)
  {
    return false;
  }

  if (x_fs_path_is_absolute_cstr(path))
  {
    if (!x_fs_path_set(out_path, path))
    {
      return false;
    }
  }
  else if (!x_fs_path(
               out_path, x_fs_path_cstr(&source->runtree_path), path))
  {
    return false;
  }

  x_fs_path_normalize(out_path);
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
  if (!x_fs_path_set(&source->runtree_path, runtree_path))
  {
    return false;
  }

  x_fs_path_normalize(&source->runtree_path);
  return true;
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

LDKAssetSourcePackage *ldk_asset_source_package_open(
    LDKAssetSource *source, const char *path)
{
  XFSPath physical_path;
  LDKPackage *package;
  LDKAssetSourcePackage *node;

  if (!source || !s_asset_source_physical_path(source, path, &physical_path))
  {
    return NULL;
  }

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
      return true;
    }
    link = &(*link)->next;
  }

  return false;
}

bool ldk_asset_source_find(const LDKAssetSource *source, const char *path,
    LDKAssetSourceFile *out_file)
{
  LDKAssetSourcePackage *node;
  const LDKPackageEntry *entry;
  XFSPath filesystem_path;
  FSFileStat stat;

  if (!source || !out_file || !ldk_package_path_is_valid(path))
  {
    return false;
  }

  memset(out_file, 0, sizeof(*out_file));

  node = source->packages;
  while (node)
  {
    entry = ldk_package_entry_find(node->package, path);
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
          x_fs_path_cstr(&source->runtree_path), path))
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
