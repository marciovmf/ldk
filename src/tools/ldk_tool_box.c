#include <stdx/stdx_common.h>

#define X_IMPL_STRING
#define X_IMPL_FILESYSTEM
#include <stdx/stdx_filesystem.h>

#include <ldk_package.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BOX_INITIAL_CAPACITY
#define BOX_INITIAL_CAPACITY 16u
#endif

typedef struct BoxRule
{
  char *pattern;
  bool exclude;
} BoxRule;

typedef struct BoxFile
{
  char *source_path;
  char *package_path;
} BoxFile;

typedef struct BoxPackage
{
  char *name;
  BoxRule *rules;
  u32 rule_count;
  u32 rule_capacity;
  BoxFile *files;
  u32 file_count;
  u32 file_capacity;
  char *output_path;
  char *temporary_path;
  char *backup_path;
  bool backed_up;
  bool installed;
} BoxPackage;

typedef struct BoxPackCommand
{
  char *root;
  char *output;
  BoxPackage *packages;
  u32 package_count;
  u32 package_capacity;
} BoxPackCommand;

static char *s_string_copy_n(const char *text, size_t length)
{
  char *copy = malloc(length + 1u);
  if (!copy)
  {
    return NULL;
  }

  memcpy(copy, text, length);
  copy[length] = 0;
  return copy;
}

static char *s_string_copy(const char *text)
{
  return text ? s_string_copy_n(text, strlen(text)) : NULL;
}

static char *s_path_join(const char *a, const char *b)
{
  size_t a_length;
  size_t b_length;
  bool separator;
  char *result;

  if (!a || !b)
  {
    return NULL;
  }

  a_length = strlen(a);
  b_length = strlen(b);
  separator = a_length > 0 && a[a_length - 1] != '/' && a[a_length - 1] != '\\';
  result = malloc(a_length + b_length + (separator ? 2u : 1u));
  if (!result)
  {
    return NULL;
  }

  memcpy(result, a, a_length);
  if (separator)
  {
    result[a_length++] = '/';
  }
  memcpy(result + a_length, b, b_length + 1u);
  return result;
}

static bool s_case_equal(const char *a, const char *b)
{
  if (!a || !b)
  {
    return false;
  }

  while (*a && *b)
  {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
    {
      return false;
    }
    ++a;
    ++b;
  }

  return *a == *b;
}

static bool s_package_name_valid(const char *name)
{
  size_t length;
  size_t extension_length = strlen(LDK_PACKAGE_FILE_EXTENSION);

  if (!name || !name[0])
  {
    return false;
  }

  length = strlen(name);
  if (length <= extension_length ||
      strcmp(name + length - extension_length, LDK_PACKAGE_FILE_EXTENSION) != 0)
  {
    return false;
  }

  for (const char *p = name; *p; ++p)
  {
    unsigned char c = (unsigned char)*p;
    if (!(isalnum(c) || c == '_' || c == '-' || c == '.'))
    {
      return false;
    }
  }

  return true;
}

static bool s_rule_pattern_valid(const char *pattern)
{
  u32 star_count = 0;

  if (!pattern || !pattern[0] || pattern[0] == '/' || pattern[0] == '\\')
  {
    return false;
  }

  for (const char *p = pattern; *p; ++p)
  {
    unsigned char c = (unsigned char)*p;
    if (c < 32u || c == ':' || c == '?' || c == '"' || c == '<' ||
        c == '>' || c == '|' || c == ';')
    {
      return false;
    }

    if (c == '*')
    {
      ++star_count;
      if (star_count > 2u)
      {
        return false;
      }
    }
    else
    {
      star_count = 0;
    }
  }

  const char *component = pattern;
  while (*component)
  {
    const char *slash = strchr(component, '/');
    size_t length = slash ? (size_t)(slash - component) : strlen(component);
    if (length == 0 || (length == 1 && component[0] == '.') ||
        (length == 2 && component[0] == '.' && component[1] == '.'))
    {
      return false;
    }
    component = slash ? slash + 1 : component + length;
  }

  return true;
}

static bool s_glob_match(const char *pattern, const char *path)
{
  while (*pattern)
  {
    if (pattern[0] == '*' && pattern[1] == '*')
    {
      pattern += 2;

      if (*pattern == '/')
      {
        ++pattern;
        if (s_glob_match(pattern, path))
        {
          return true;
        }

        for (const char *p = path; *p; ++p)
        {
          if (*p == '/' && s_glob_match(pattern, p + 1))
          {
            return true;
          }
        }
        return false;
      }

      if (!*pattern)
      {
        return true;
      }

      for (const char *p = path;; ++p)
      {
        if (s_glob_match(pattern, p))
        {
          return true;
        }
        if (!*p)
        {
          break;
        }
      }
      return false;
    }

    if (*pattern == '*')
    {
      ++pattern;
      for (const char *p = path;; ++p)
      {
        if (s_glob_match(pattern, p))
        {
          return true;
        }
        if (!*p || *p == '/')
        {
          break;
        }
      }
      return false;
    }

    if (*path != *pattern)
    {
      return false;
    }

    ++pattern;
    ++path;
  }

  return *path == 0;
}

static bool s_package_matches(const BoxPackage *package, const char *path)
{
  bool included = false;
  bool excluded = false;

  for (u32 i = 0; i < package->rule_count; ++i)
  {
    const BoxRule *rule = &package->rules[i];
    if (s_glob_match(rule->pattern, path))
    {
      if (rule->exclude)
      {
        excluded = true;
      }
      else
      {
        included = true;
      }
    }
  }

  return included && !excluded;
}

static bool s_package_rules_reserve(BoxPackage *package, u32 required)
{
  if (required <= package->rule_capacity)
  {
    return true;
  }

  u32 capacity = package->rule_capacity
                     ? package->rule_capacity
                     : BOX_INITIAL_CAPACITY;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }

  BoxRule *rules = realloc(package->rules, sizeof(*rules) * (size_t)capacity);
  if (!rules)
  {
    return false;
  }

  package->rules = rules;
  package->rule_capacity = capacity;
  return true;
}

static bool s_package_files_reserve(BoxPackage *package, u32 required)
{
  if (required <= package->file_capacity)
  {
    return true;
  }

  u32 capacity = package->file_capacity
                     ? package->file_capacity
                     : BOX_INITIAL_CAPACITY;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }

  BoxFile *files = realloc(package->files, sizeof(*files) * (size_t)capacity);
  if (!files)
  {
    return false;
  }

  package->files = files;
  package->file_capacity = capacity;
  return true;
}

static bool s_command_packages_reserve(BoxPackCommand *command, u32 required)
{
  if (required <= command->package_capacity)
  {
    return true;
  }

  u32 capacity = command->package_capacity ? command->package_capacity : 4u;
  while (capacity < required)
  {
    if (capacity > UINT32_MAX / 2u)
    {
      capacity = required;
      break;
    }
    capacity *= 2u;
  }

  BoxPackage *packages =
      realloc(command->packages, sizeof(*packages) * (size_t)capacity);
  if (!packages)
  {
    return false;
  }

  command->packages = packages;
  command->package_capacity = capacity;
  return true;
}

static void s_package_clear(BoxPackage *package)
{
  if (!package)
  {
    return;
  }

  for (u32 i = 0; i < package->rule_count; ++i)
  {
    free(package->rules[i].pattern);
  }
  for (u32 i = 0; i < package->file_count; ++i)
  {
    free(package->files[i].source_path);
    free(package->files[i].package_path);
  }

  free(package->rules);
  free(package->files);
  free(package->name);
  free(package->output_path);
  free(package->temporary_path);
  free(package->backup_path);
  memset(package, 0, sizeof(*package));
}

static void s_command_clear(BoxPackCommand *command)
{
  if (!command)
  {
    return;
  }

  for (u32 i = 0; i < command->package_count; ++i)
  {
    s_package_clear(&command->packages[i]);
  }

  free(command->packages);
  free(command->root);
  free(command->output);
  memset(command, 0, sizeof(*command));
}

static bool s_package_rule_add(
    BoxPackage *package, const char *begin, size_t length)
{
  bool exclude = false;
  char *pattern;

  while (length && isspace((unsigned char)*begin))
  {
    ++begin;
    --length;
  }
  while (length && isspace((unsigned char)begin[length - 1u]))
  {
    --length;
  }

  if (!length)
  {
    return false;
  }

  if (*begin == '!')
  {
    exclude = true;
    ++begin;
    --length;
    while (length && isspace((unsigned char)*begin))
    {
      ++begin;
      --length;
    }
  }

  if (!length)
  {
    return false;
  }

  pattern = s_string_copy_n(begin, length);
  if (!pattern)
  {
    return false;
  }

  for (char *p = pattern; *p; ++p)
  {
    if (*p == '\\')
    {
      *p = '/';
    }
  }

  if (!s_rule_pattern_valid(pattern) ||
      !s_package_rules_reserve(package, package->rule_count + 1u))
  {
    free(pattern);
    return false;
  }

  package->rules[package->rule_count++] =
      (BoxRule){.pattern = pattern, .exclude = exclude};
  return true;
}

static bool s_package_rules_parse(BoxPackage *package, const char *text)
{
  if (!text || !text[0])
  {
    return true;
  }

  const char *begin = text;
  while (true)
  {
    const char *separator = strchr(begin, '|');
    const char *end = separator ? separator : begin + strlen(begin);
    if (!s_package_rule_add(package, begin, (size_t)(end - begin)))
    {
      return false;
    }

    if (!separator)
    {
      break;
    }
    begin = separator + 1;
  }

  return true;
}

static bool s_command_package_add(
    BoxPackCommand *command, const char *name, const char *rules)
{
  if (!s_package_name_valid(name) ||
      !s_command_packages_reserve(command, command->package_count + 1u))
  {
    return false;
  }

  for (u32 i = 0; i < command->package_count; ++i)
  {
    if (s_case_equal(command->packages[i].name, name))
    {
      return false;
    }
  }

  BoxPackage package = {0};
  package.name = s_string_copy(name);
  if (!package.name || !s_package_rules_parse(&package, rules))
  {
    s_package_clear(&package);
    return false;
  }

  command->packages[command->package_count++] = package;
  return true;
}

static bool s_pack_arguments_parse(
    int argc, char **argv, BoxPackCommand *command)
{
  for (int i = 2; i < argc;)
  {
    if (strcmp(argv[i], "--root") == 0 && i + 1 < argc)
    {
      if (command->root)
      {
        return false;
      }
      command->root = s_string_copy(argv[i + 1]);
      i += 2;
      continue;
    }

    if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
    {
      if (command->output)
      {
        return false;
      }
      command->output = s_string_copy(argv[i + 1]);
      i += 2;
      continue;
    }

    if (strcmp(argv[i], "--package") == 0 && i + 1 < argc)
    {
      const char *rules = "";
      int consumed = 2;
      if (i + 2 < argc && strcmp(argv[i + 2], "--package") != 0 &&
          strcmp(argv[i + 2], "--root") != 0 &&
          strcmp(argv[i + 2], "--output") != 0)
      {
        rules = argv[i + 2];
        consumed = 3;
      }

      if (!s_command_package_add(command, argv[i + 1], rules))
      {
        return false;
      }
      i += consumed;
      continue;
    }

    return false;
  }

  return command->root && command->root[0] && command->output &&
         command->output[0] && command->package_count > 0;
}

static bool s_package_file_add(
    BoxPackage *package, const char *source_path, const char *package_path)
{
  if (!s_package_files_reserve(package, package->file_count + 1u))
  {
    return false;
  }

  BoxFile file = {
      .source_path = s_string_copy(source_path),
      .package_path = s_string_copy(package_path),
  };
  if (!file.source_path || !file.package_path)
  {
    free(file.source_path);
    free(file.package_path);
    return false;
  }

  package->files[package->file_count++] = file;
  return true;
}

static bool s_collect_file(
    BoxPackCommand *command, const char *source_path, const char *package_path)
{
  i32 matched_package = -1;

  for (u32 i = 0; i < command->package_count; ++i)
  {
    if (!s_package_matches(&command->packages[i], package_path))
    {
      continue;
    }

    if (matched_package >= 0)
    {
      fprintf(stderr,
          "Package conflict: '%s' matches both '%s' and '%s'.\n",
          package_path, command->packages[(u32)matched_package].name,
          command->packages[i].name);
      return false;
    }

    matched_package = (i32)i;
  }

  if (matched_package < 0)
  {
    return true;
  }

  return s_package_file_add(
      &command->packages[(u32)matched_package], source_path, package_path);
}

static bool s_collect_directory(
    BoxPackCommand *command, const char *directory, const char *relative)
{
  XFSDireEntry entry = {0};
  XFSDireHandle *handle = x_fs_find_first_file(directory, &entry);

  if (!handle)
  {
    return false;
  }

  bool result = true;
  while (true)
  {
    if (strcmp(entry.name, ".") != 0 && strcmp(entry.name, "..") != 0)
    {
      char *source_path = s_path_join(directory, entry.name);
      char *package_path = relative && relative[0]
                               ? s_path_join(relative, entry.name)
                               : s_string_copy(entry.name);

      if (!source_path || !package_path)
      {
        free(source_path);
        free(package_path);
        result = false;
        break;
      }

      for (char *p = package_path; *p; ++p)
      {
        if (*p == '\\')
        {
          *p = '/';
        }
      }

      if (strlen(package_path) > LDK_PACKAGE_PATH_MAX_LENGTH)
      {
        fprintf(stderr, "Package path is too long: '%s'.\n", package_path);
        result = false;
      }
      else if (entry.is_directory)
      {
        result = s_collect_directory(command, source_path, package_path);
      }
      else
      {
        result = s_collect_file(command, source_path, package_path);
      }

      free(source_path);
      free(package_path);
      if (!result)
      {
        break;
      }
    }

    if (!x_fs_find_next_file(handle, &entry))
    {
      break;
    }
  }

  x_fs_find_close(handle);
  return result;
}

static int s_file_compare(const void *a, const void *b)
{
  const BoxFile *fa = a;
  const BoxFile *fb = b;
  return strcmp(fa->package_path, fb->package_path);
}

static bool s_package_output_paths_prepare(
    BoxPackCommand *command, BoxPackage *package)
{
  package->output_path = s_path_join(command->output, package->name);
  if (!package->output_path)
  {
    return false;
  }

  size_t output_length = strlen(package->output_path);
  package->temporary_path = malloc(output_length + sizeof(".tmp"));
  package->backup_path = malloc(output_length + sizeof(".bak"));
  if (!package->temporary_path || !package->backup_path)
  {
    return false;
  }

  snprintf(package->temporary_path, output_length + sizeof(".tmp"),
      "%s.tmp", package->output_path);
  snprintf(package->backup_path, output_length + sizeof(".bak"),
      "%s.bak", package->output_path);

  if (x_fs_path_exists_cstr(package->backup_path))
  {
    fprintf(stderr,
        "Package backup already exists: '%s'. Recover or remove it first.\n",
        package->backup_path);
    return false;
  }

  return true;
}

static bool s_package_write(BoxPackage *package)
{
  remove(package->temporary_path);
  LDKPackageWriter *writer =
      ldk_package_writer_create(package->temporary_path);
  if (!writer)
  {
    return false;
  }

  bool result = true;
  if (package->file_count > 1)
  {
    qsort(package->files, package->file_count, sizeof(*package->files),
        s_file_compare);
  }

  for (u32 i = 0; i < package->file_count; ++i)
  {
    BoxFile *file = &package->files[i];
    if (!ldk_package_writer_add_file(writer, file->source_path,
            file->package_path, LDK_PACKAGE_COMPRESSION_NONE))
    {
      result = false;
      break;
    }
  }

  if (result)
  {
    result = ldk_package_writer_finalize(writer);
  }
  ldk_package_writer_destroy(writer);
  return result;
}

static void s_install_rollback(BoxPackCommand *command)
{
  for (u32 i = 0; i < command->package_count; ++i)
  {
    BoxPackage *package = &command->packages[i];
    if (package->installed)
    {
      remove(package->output_path);
      package->installed = false;
    }
  }

  for (u32 i = 0; i < command->package_count; ++i)
  {
    BoxPackage *package = &command->packages[i];
    if (package->backed_up)
    {
      rename(package->backup_path, package->output_path);
      package->backed_up = false;
    }
    remove(package->temporary_path);
  }
}

static bool s_packages_install(BoxPackCommand *command)
{
  for (u32 i = 0; i < command->package_count; ++i)
  {
    BoxPackage *package = &command->packages[i];

    if (x_fs_path_exists_cstr(package->output_path))
    {
      if (rename(package->output_path, package->backup_path) != 0)
      {
        s_install_rollback(command);
        return false;
      }
      package->backed_up = true;
    }
  }

  for (u32 i = 0; i < command->package_count; ++i)
  {
    BoxPackage *package = &command->packages[i];
    if (rename(package->temporary_path, package->output_path) != 0)
    {
      s_install_rollback(command);
      return false;
    }
    package->installed = true;
  }

  for (u32 i = 0; i < command->package_count; ++i)
  {
    BoxPackage *package = &command->packages[i];
    if (package->backed_up)
    {
      remove(package->backup_path);
      package->backed_up = false;
    }
    package->installed = false;
  }

  return true;
}

static int s_pack(int argc, char **argv)
{
  BoxPackCommand command = {0};
  int result = 1;

  if (!s_pack_arguments_parse(argc, argv, &command))
  {
    fprintf(stderr, "Invalid pack arguments.\n");
    goto done;
  }

  if (!x_fs_path_is_directory_cstr(command.root))
  {
    fprintf(stderr, "Pack root is not a directory: '%s'.\n", command.root);
    goto done;
  }

  if (!x_fs_directory_create_recursive(command.output))
  {
    fprintf(stderr,
        "Could not create output directory '%s'.\n", command.output);
    goto done;
  }

  if (!s_collect_directory(&command, command.root, ""))
  {
    goto done;
  }

  for (u32 i = 0; i < command.package_count; ++i)
  {
    BoxPackage *package = &command.packages[i];
    if (!s_package_output_paths_prepare(&command, package) ||
        !s_package_write(package))
    {
      fprintf(stderr, "Failed to create package '%s'.\n", package->name);
      for (u32 j = 0; j < command.package_count; ++j)
      {
        if (command.packages[j].temporary_path)
        {
          remove(command.packages[j].temporary_path);
        }
      }
      goto done;
    }
  }

  if (!s_packages_install(&command))
  {
    fprintf(stderr, "Failed to install generated packages.\n");
    goto done;
  }

  for (u32 i = 0; i < command.package_count; ++i)
  {
    printf("%s: %u file(s)\n", command.packages[i].name,
        command.packages[i].file_count);
  }
  result = 0;

done:
  s_command_clear(&command);
  return result;
}

static const char *s_compression_name(LDKPackageCompression compression)
{
  switch (compression)
  {
  case LDK_PACKAGE_COMPRESSION_NONE:
    return "none";
  default:
    return "unsupported";
  }
}

static void s_usage(const char *program)
{
  printf("Usage:\n");
  printf("  %s pack --root <directory> --output <directory>\n", program);
  printf("      --package <file.box> <rules> "
         "[--package <file.box> <rules> ...]\n");
  printf("  %s list <package.box>\n", program);
  printf("  %s extract <package.box> <entry> <destination>\n", program);
  printf("  %s extract-all <package.box> <destination-directory>\n", program);
}

static int s_list(const char *package_path)
{
  LDKPackage *package = ldk_package_open(package_path);
  if (!package)
  {
    fprintf(stderr, "Failed to open package '%s'.\n", package_path);
    return 1;
  }

  u32 count = ldk_package_entry_count(package);
  for (u32 i = 0; i < count; ++i)
  {
    const LDKPackageEntry *entry = ldk_package_entry_at(package, i);
    LDKPackageCompression compression =
        ldk_package_entry_get_compression(entry);

    printf("%10llu  %10llu  %-11s  %s\n",
        (unsigned long long)ldk_package_entry_get_size(entry),
        (unsigned long long)ldk_package_entry_get_stored_size(entry),
        s_compression_name(compression), ldk_package_entry_get_path(entry));
  }

  ldk_package_close(package);
  return 0;
}

static int s_extract(const char *package_path, const char *entry_path,
    const char *destination_path)
{
  LDKPackage *package = ldk_package_open(package_path);
  if (!package)
  {
    fprintf(stderr, "Failed to open package '%s'.\n", package_path);
    return 1;
  }

  const LDKPackageEntry *entry = ldk_package_entry_find(package, entry_path);
  if (!entry)
  {
    fprintf(stderr, "Entry '%s' was not found.\n", entry_path);
    ldk_package_close(package);
    return 1;
  }

  bool ok = ldk_package_entry_extract(package, entry, destination_path);
  if (!ok)
  {
    fprintf(stderr, "Failed to extract '%s'.\n", entry_path);
  }

  ldk_package_close(package);
  return ok ? 0 : 1;
}

static int s_extract_all(
    const char *package_path, const char *destination_directory)
{
  LDKPackage *package = ldk_package_open(package_path);
  if (!package)
  {
    fprintf(stderr, "Failed to open package '%s'.\n", package_path);
    return 1;
  }

  bool ok = ldk_package_extract_all(package, destination_directory);
  if (!ok)
  {
    fprintf(stderr, "Failed to extract package '%s'.\n", package_path);
  }

  ldk_package_close(package);
  return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
  if (argc >= 2 && strcmp(argv[1], "pack") == 0)
  {
    return s_pack(argc, argv);
  }

  if (argc == 3 && strcmp(argv[1], "list") == 0)
  {
    return s_list(argv[2]);
  }

  if (argc == 5 && strcmp(argv[1], "extract") == 0)
  {
    return s_extract(argv[2], argv[3], argv[4]);
  }

  if (argc == 4 && strcmp(argv[1], "extract-all") == 0)
  {
    return s_extract_all(argv[2], argv[3]);
  }

  s_usage(argv[0]);
  return 1;
}
