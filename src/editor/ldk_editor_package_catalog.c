#include "ldk_editor_package_catalog.h"
#include "ldk_ui_drag_n_drop.h"
#include "module/ldk_ui.h"
#include <ldk_package.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LDKEditorPackageRule
{
  XSmallstr text;
} LDKEditorPackageRule;

typedef struct LDKEditorPackageDefinition
{
  XSmallstr name;
  XArray *rules;
} LDKEditorPackageDefinition;

typedef struct LDKEditorPackageCatalogState
{
  XFSPath project_path;
  XArray *packages;
  u32 selected;
  LDKUIPoint scroll;
  char package_input[X_SMALLSTR_MAX_LENGTH + 1];
  char rule_input[X_SMALLSTR_MAX_LENGTH + 1];
  char error[256];
  bool loaded;
  bool close_requested;
} LDKEditorPackageCatalogState;

static LDKEditorPackageCatalogState s_package_catalog = {
    .selected = UINT32_MAX,
};
static bool s_package_catalog_window_registered = false;

static void s_package_definition_terminate(LDKEditorPackageDefinition *package)
{
  if (!package)
  {
    return;
  }

  if (package->rules)
  {
    x_array_destroy(package->rules);
  }

  memset(package, 0, sizeof(*package));
}

static void s_package_catalog_clear(void)
{
  if (s_package_catalog.packages)
  {
    for (u32 i = 0; i < x_array_count(s_package_catalog.packages); ++i)
    {
      LDKEditorPackageDefinition *package =
          x_array_get(s_package_catalog.packages, i);
      s_package_definition_terminate(package);
    }
    x_array_destroy(s_package_catalog.packages);
  }

  memset(&s_package_catalog, 0, sizeof(s_package_catalog));
  s_package_catalog.selected = UINT32_MAX;
}

static bool s_package_catalog_same_project(const LDKEditorContext *editor)
{
  if (!editor || !editor->project.loaded || !s_package_catalog.loaded)
  {
    return false;
  }

  XFSPath current = editor->project.project_file_path;
  XFSPath cached = s_package_catalog.project_path;
  x_fs_path_normalize(&current);
  x_fs_path_normalize(&cached);
  return x_fs_path_compare(&current, &cached) == 0;
}

static bool s_package_name_equal(const char *a, const char *b)
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

  if (!name || !name[0])
  {
    return false;
  }

  length = strlen(name);
  size_t extension_length = strlen(LDK_PACKAGE_FILE_EXTENSION);
  if (length <= extension_length ||
      strcmp(name + length - extension_length,
          LDK_PACKAGE_FILE_EXTENSION) != 0)
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

static bool s_package_rule_normalize(
    const char *source, XSmallstr *out_rule)
{
  const char *begin;
  const char *end;
  bool exclude = false;
  char buffer[X_SMALLSTR_MAX_LENGTH + 1];
  size_t length = 0;
  const char *component;

  if (!source || !out_rule)
  {
    return false;
  }

  begin = source;
  while (*begin && isspace((unsigned char)*begin))
  {
    ++begin;
  }

  end = begin + strlen(begin);
  while (end > begin && isspace((unsigned char)end[-1]))
  {
    --end;
  }

  if (begin == end)
  {
    return false;
  }

  if (*begin == '!')
  {
    exclude = true;
    ++begin;
    while (begin < end && isspace((unsigned char)*begin))
    {
      ++begin;
    }
  }

  if (begin == end || *begin == '/' || *begin == '\\')
  {
    return false;
  }

  if (exclude)
  {
    buffer[length++] = '!';
  }

  for (const char *p = begin; p < end; ++p)
  {
    char c = *p == '\\' ? '/' : *p;
    if (c == ':' || c == '\n' || c == '\r' || c == '\t' ||
        c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == ';' ||
        (unsigned char)c < 32u)
    {
      return false;
    }

    if (length >= X_SMALLSTR_MAX_LENGTH)
    {
      return false;
    }
    buffer[length++] = c;
  }
  if (length > (exclude ? 1u : 0u) && buffer[length - 1] == '/')
  {
    if (length + 2 > X_SMALLSTR_MAX_LENGTH)
    {
      return false;
    }
    buffer[length++] = '*';
    buffer[length++] = '*';
  }
  buffer[length] = 0;

  component = buffer + (exclude ? 1 : 0);
  while (*component)
  {
    const char *slash = strchr(component, '/');
    size_t component_length = slash ? (size_t)(slash - component)
                                    : strlen(component);
    if (component_length == 0 ||
        (component_length == 1 && component[0] == '.') ||
        (component_length == 2 && component[0] == '.' && component[1] == '.'))
    {
      return false;
    }

    if (!slash)
    {
      break;
    }
    component = slash + 1;
  }

  x_smallstr_from_cstr(out_rule, buffer);
  return out_rule->length == length;
}

static bool s_package_rule_add(
    LDKEditorPackageDefinition *package, const char *rule)
{
  LDKEditorPackageRule entry = {0};

  if (!package || !package->rules ||
      !s_package_rule_normalize(rule, &entry.text))
  {
    return false;
  }

  for (u32 i = 0; i < x_array_count(package->rules); ++i)
  {
    LDKEditorPackageRule *existing = x_array_get(package->rules, i);
    if (strcmp(existing->text.buf, entry.text.buf) == 0)
    {
      return false;
    }
  }

  return x_array_add(package->rules, &entry) == 0;
}

static bool s_package_definition_add(const char *name)
{
  LDKEditorPackageDefinition package = {0};

  if (!s_package_catalog.packages || !s_package_name_valid(name))
  {
    return false;
  }

  for (u32 i = 0; i < x_array_count(s_package_catalog.packages); ++i)
  {
    LDKEditorPackageDefinition *existing =
        x_array_get(s_package_catalog.packages, i);
    if (s_package_name_equal(existing->name.buf, name))
    {
      return false;
    }
  }

  package.rules = x_array_create(sizeof(LDKEditorPackageRule), 8);
  if (!package.rules)
  {
    return false;
  }

  x_smallstr_from_cstr(&package.name, name);
  if (package.name.length != strlen(name) ||
      x_array_add(s_package_catalog.packages, &package) != 0)
  {
    s_package_definition_terminate(&package);
    return false;
  }

  return true;
}

static bool s_package_rules_parse(
    LDKEditorPackageDefinition *package, const char *rules)
{
  const char *begin = rules ? rules : "";

  while (*begin)
  {
    const char *end = strchr(begin, '|');
    size_t length = end ? (size_t)(end - begin) : strlen(begin);
    char rule[X_SMALLSTR_MAX_LENGTH + 1];

    if (length > X_SMALLSTR_MAX_LENGTH)
    {
      return false;
    }

    memcpy(rule, begin, length);
    rule[length] = 0;

    const char *trim = rule;
    while (*trim && isspace((unsigned char)*trim))
    {
      ++trim;
    }

    if (*trim && !s_package_rule_add(package, trim))
    {
      return false;
    }

    if (!end)
    {
      break;
    }
    begin = end + 1;
  }

  return true;
}

static void s_package_catalog_error_set(const char *message)
{
  snprintf(s_package_catalog.error, sizeof(s_package_catalog.error), "%s",
      message ? message : "Package catalog error.");
}

static bool s_package_catalog_load(LDKEditorContext *editor)
{
  XIni ini = {0};
  XIniError ini_error = {0};

  if (!editor || !editor->project.loaded)
  {
    s_package_catalog_clear();
    return false;
  }

  if (s_package_catalog_same_project(editor))
  {
    return true;
  }

  s_package_catalog_clear();
  s_package_catalog.packages =
      x_array_create(sizeof(LDKEditorPackageDefinition), 8);
  if (!s_package_catalog.packages)
  {
    s_package_catalog_error_set("Could not allocate package catalog.");
    return false;
  }

  s_package_catalog.project_path = editor->project.project_file_path;

  if (!x_ini_load_file(editor->project.project_file_path.buf, &ini, &ini_error))
  {
    snprintf(s_package_catalog.error, sizeof(s_package_catalog.error),
        "Could not read package catalog: %s",
        ini_error.message ? ini_error.message : "invalid project file");
    return false;
  }

  for (i32 section_i = 0; section_i < x_ini_section_count(&ini); ++section_i)
  {
    const char *section = x_ini_section_name(&ini, section_i);
    if (!section || strcmp(section, ".packages") != 0)
    {
      continue;
    }

    i32 key_count = x_ini_key_count(&ini, section_i);
    for (i32 key_i = 0; key_i < key_count; ++key_i)
    {
      const char *name = x_ini_key_name(&ini, section_i, key_i);
      const char *rules = x_ini_value_at(&ini, section_i, key_i);

      if (!name || !s_package_definition_add(name))
      {
        snprintf(s_package_catalog.error, sizeof(s_package_catalog.error),
            "Invalid or duplicate package name '%s'.", name ? name : "");
        x_ini_free(&ini);
        return false;
      }

      LDKEditorPackageDefinition *package = x_array_get(
          s_package_catalog.packages,
          x_array_count(s_package_catalog.packages) - 1);
      if (!s_package_rules_parse(package, rules))
      {
        snprintf(s_package_catalog.error, sizeof(s_package_catalog.error),
            "Invalid or duplicate rule in package '%s'.", package->name.buf);
        x_ini_free(&ini);
        return false;
      }
    }
  }

  x_ini_free(&ini);
  if (x_array_count(s_package_catalog.packages) > 0)
  {
    s_package_catalog.selected = 0;
  }
  s_package_catalog.loaded = true;
  return true;
}

static bool s_package_catalog_file_path_with_suffix(
    const XFSPath *source, const char *suffix, XFSPath *out)
{
  size_t length;
  size_t suffix_length;
  char buffer[sizeof(out->buf)];

  if (!source || !suffix || !out)
  {
    return false;
  }

  length = strlen(source->buf);
  suffix_length = strlen(suffix);
  if (length + suffix_length >= sizeof(buffer))
  {
    return false;
  }

  memcpy(buffer, source->buf, length);
  memcpy(buffer + length, suffix, suffix_length + 1);
  x_fs_path_set(out, buffer);
  return true;
}

static bool s_package_catalog_line_is_section(
    const char *line, const char *end)
{
  while (line < end && (*line == ' ' || *line == '\t'))
  {
    ++line;
  }

  return line < end && *line == '[' &&
         memchr(line, ']', (size_t)(end - line)) != NULL;
}

static bool s_package_catalog_section_is_packages(
    const char *line, const char *end)
{
  const char *p = line;
  const char *close;

  while (p < end && (*p == ' ' || *p == '\t'))
  {
    ++p;
  }
  if (p >= end || *p != '[')
  {
    return false;
  }

  close = memchr(p, ']', (size_t)(end - p));
  if (!close)
  {
    return false;
  }

  ++p;
  while (p < close && isspace((unsigned char)*p))
  {
    ++p;
  }
  while (close > p && isspace((unsigned char)close[-1]))
  {
    --close;
  }

  return close - p == 9 && memcmp(p, ".packages", 9) == 0;
}

static bool s_package_catalog_manifest_save(LDKEditorContext *editor)
{
  XFSPath path;
  XFSPath temporary = {0};
  XFSPath backup = {0};
  FILE *in = NULL;
  FILE *out = NULL;
  char *source = NULL;
  bool skip = false;
  bool backed_up = false;
  bool installed = false;
  bool ok = false;
  long file_size;

  if (!editor || !editor->project.loaded || !s_package_catalog.packages)
  {
    return false;
  }

  path = editor->project.project_file_path;
  if (!s_package_catalog_file_path_with_suffix(
          &path, ".packages.tmp", &temporary) ||
      !s_package_catalog_file_path_with_suffix(
          &path, ".packages.bak", &backup))
  {
    s_package_catalog_error_set(
        "Project path is too long to save the package catalog.");
    return false;
  }

  if (x_fs_path_exists_cstr(temporary.buf) || x_fs_path_exists_cstr(backup.buf))
  {
    s_package_catalog_error_set(
        "Package catalog save blocked by stale .packages.tmp/.packages.bak "
        "files.");
    return false;
  }

  in = fopen(path.buf, "rb");
  if (!in || fseek(in, 0, SEEK_END) != 0)
  {
    goto done;
  }

  file_size = ftell(in);
  if (file_size < 0 || fseek(in, 0, SEEK_SET) != 0)
  {
    goto done;
  }

  source = malloc((size_t)file_size + 1u);
  if (!source || fread(source, 1, (size_t)file_size, in) != (size_t)file_size)
  {
    goto done;
  }
  source[file_size] = 0;
  fclose(in);
  in = NULL;

  out = fopen(temporary.buf, "wbx");
  if (!out)
  {
    goto done;
  }

  const char *begin = source;
  if ((size_t)file_size >= 3 && memcmp(begin, "\xef\xbb\xbf", 3) == 0)
  {
    if (fwrite(begin, 1, 3, out) != 3)
    {
      goto done;
    }
    begin += 3;
  }

  for (const char *line = begin; *line;)
  {
    const char *end = strchr(line, '\n');
    end = end ? end + 1 : line + strlen(line);

    if (s_package_catalog_line_is_section(line, end))
    {
      skip = s_package_catalog_section_is_packages(line, end);
    }

    if (!skip && fwrite(line, 1, (size_t)(end - line), out) !=
                     (size_t)(end - line))
    {
      goto done;
    }

    line = end;
  }

  fputs("\n[.packages]\n", out);
  for (u32 package_i = 0;
       package_i < x_array_count(s_package_catalog.packages); ++package_i)
  {
    LDKEditorPackageDefinition *package =
        x_array_get(s_package_catalog.packages, package_i);
    size_t line_length = strlen(package->name.buf) + 5u;
    for (u32 rule_i = 0; rule_i < x_array_count(package->rules); ++rule_i)
    {
      LDKEditorPackageRule *rule = x_array_get(package->rules, rule_i);
      line_length += strlen(rule->text.buf) + (rule_i > 0 ? 1u : 0u);
    }
    if (line_length >= X_INI_MAX_LINE)
    {
      snprintf(s_package_catalog.error, sizeof(s_package_catalog.error),
          "Rules for package '%s' exceed the INI line limit.",
          package->name.buf);
      goto done;
    }

    fprintf(out, "%s = ", package->name.buf);
    fputc('"', out);
    for (u32 rule_i = 0; rule_i < x_array_count(package->rules); ++rule_i)
    {
      LDKEditorPackageRule *rule = x_array_get(package->rules, rule_i);
      if (rule_i > 0)
      {
        fputc('|', out);
      }
      for (const char *p = rule->text.buf; *p; ++p)
      {
        if (*p == '\\' || *p == '"')
        {
          fputc('\\', out);
        }
        fputc((unsigned char)*p, out);
      }
    }
    fputs("\"\n", out);
  }

  if (ferror(out) || fclose(out) != 0)
  {
    out = NULL;
    goto done;
  }
  out = NULL;

  {
    XIni check = {0};
    XIniError check_error = {0};
    if (!x_ini_load_file(temporary.buf, &check, &check_error))
    {
      snprintf(s_package_catalog.error, sizeof(s_package_catalog.error),
          "Generated project file is invalid: %s",
          check_error.message ? check_error.message : "INI parse error");
      goto done;
    }
    x_ini_free(&check);
  }

  if (!x_fs_file_rename(path.buf, backup.buf))
  {
    goto done;
  }
  backed_up = true;

  if (!x_fs_file_rename(temporary.buf, path.buf))
  {
    goto done;
  }
  installed = true;

  if (!x_fs_file_delete(backup.buf))
  {
    ldki_editor_log_warning(editor,
        "Package catalog saved, but its temporary backup could not be "
        "deleted.");
  }

  ok = true;

done:
  if (in)
  {
    fclose(in);
  }
  if (out)
  {
    fclose(out);
  }
  free(source);

  if (!ok)
  {
    if (installed)
    {
      x_fs_file_delete(path.buf);
    }
    if (backed_up)
    {
      x_fs_file_rename(backup.buf, path.buf);
    }
    x_fs_file_delete(temporary.buf);

    if (!s_package_catalog.error[0])
    {
      s_package_catalog_error_set("Failed to save package catalog.");
    }
  }

  return ok;
}

static bool s_package_catalog_runtree_relative(LDKEditorContext *editor,
    const XFSPath *path, XFSPath *out_relative)
{
  XFSPath runtree;
  XFSPath normalized;

  if (!editor || !editor->project.loaded || !path || !out_relative)
  {
    return false;
  }

  runtree = editor->project.run_root_path;
  normalized = *path;
  x_fs_path_normalize(&runtree);
  x_fs_path_normalize(&normalized);

  if (x_fs_path_compare(&runtree, &normalized) == 0)
  {
    memset(out_relative, 0, sizeof(*out_relative));
    return true;
  }

  if (x_fs_path_relative_to(&runtree, &normalized, out_relative) == 0)
  {
    return false;
  }
  x_fs_path_normalize(out_relative);

  if (x_fs_path_is_absolute(out_relative) ||
      strcmp(out_relative->buf, "..") == 0 ||
      strncmp(out_relative->buf, "../", 3) == 0 ||
      strncmp(out_relative->buf, "..\\", 3) == 0)
  {
    memset(out_relative, 0, sizeof(*out_relative));
    return false;
  }

  return true;
}

static bool s_package_catalog_path_rule(LDKEditorContext *editor,
    const XFSPath *path, bool is_directory, XSmallstr *out_rule)
{
  XFSPath relative = {0};
  char buffer[X_SMALLSTR_MAX_LENGTH + 1];

  if (!s_package_catalog_runtree_relative(editor, path, &relative))
  {
    return false;
  }

  if (relative.length == 0)
  {
    if (!is_directory)
    {
      return false;
    }
    x_smallstr_from_cstr(out_rule, "**");
    return true;
  }

  for (size_t i = 0; relative.buf[i]; ++i)
  {
    if (relative.buf[i] == '\\')
    {
      relative.buf[i] = '/';
    }
  }

  if (is_directory)
  {
    int written = snprintf(buffer, sizeof(buffer), "%s/**", relative.buf);
    if (written <= 0 || (size_t)written >= sizeof(buffer))
    {
      return false;
    }
    return s_package_rule_normalize(buffer, out_rule);
  }

  return s_package_rule_normalize(relative.buf, out_rule);
}

static void s_package_catalog_window_register(LDKEditorContext *editor)
{
  if (s_package_catalog_window_registered || !editor)
  {
    return;
  }

  LDKEditorWindow window = {.id = LDK_EDITOR_WINDOW_PACKAGE_CATALOG,
      .title = "Packages",
      .function = ldki_editor_package_catalog_show,
      .data = NULL};

  if (ldk_editor_window_add((LDKEditor *)editor, &window))
  {
    s_package_catalog_window_registered = true;
    ldki_editor_window_hide(LDK_EDITOR_WINDOW_PACKAGE_CATALOG);
  }
}

void ldki_editor_package_catalog_sync(LDKEditorContext *editor)
{
  if (!editor)
  {
    return;
  }

  s_package_catalog_window_register(editor);

  if (!editor->project.loaded)
  {
    s_package_catalog_clear();
    return;
  }

  if (s_package_catalog.close_requested)
  {
    ldki_editor_window_hide(LDK_EDITOR_WINDOW_PACKAGE_CATALOG);
    s_package_catalog_clear();
    return;
  }

  if (!ldki_editor_window_is_open(LDK_EDITOR_WINDOW_PACKAGE_CATALOG))
  {
    s_package_catalog_clear();
  }
}

void ldki_editor_package_catalog_open(LDKEditorContext *editor)
{
  if (!editor || !editor->project.loaded)
  {
    return;
  }

  s_package_catalog_window_register(editor);
  s_package_catalog_load(editor);
  ldki_editor_window_show(LDK_EDITOR_WINDOW_PACKAGE_CATALOG);
}

u32 ldki_editor_package_catalog_count(LDKEditorContext *editor)
{
  if (!s_package_catalog_load(editor) || !s_package_catalog.packages)
  {
    return 0;
  }

  return x_array_count(s_package_catalog.packages);
}

const char *ldki_editor_package_catalog_name_get(
    LDKEditorContext *editor, u32 index)
{
  if (!s_package_catalog_load(editor) || !s_package_catalog.packages ||
      index >= x_array_count(s_package_catalog.packages))
  {
    return NULL;
  }

  LDKEditorPackageDefinition *package =
      x_array_get(s_package_catalog.packages, index);
  return package->name.buf;
}

bool ldki_editor_package_catalog_path_is_packageable(
    LDKEditorContext *editor, const XFSPath *path)
{
  XFSPath relative = {0};

  if (!editor || !editor->project.loaded || !path ||
      (!x_fs_path_is_file(path) && !x_fs_path_is_directory(path)))
  {
    return false;
  }

  return s_package_catalog_runtree_relative(editor, path, &relative);
}

bool ldki_editor_package_catalog_add_path(LDKEditorContext *editor,
    u32 package_index, const XFSPath *path, bool is_directory)
{
  XSmallstr rule = {0};

  if (!editor || editor->project_build.active ||
      editor->editor_state != LDK_EDITOR_STATE_STOPED)
  {
    s_package_catalog_error_set(
        "Package editing is available only in STOP, outside a build.");
    return false;
  }

  if (!s_package_catalog_load(editor) || !s_package_catalog.packages ||
      package_index >= x_array_count(s_package_catalog.packages) ||
      !s_package_catalog_path_rule(editor, path, is_directory, &rule))
  {
    s_package_catalog_error_set(
        "Only files and folders inside the project runtree can be packaged.");
    return false;
  }

  LDKEditorPackageDefinition *package =
      x_array_get(s_package_catalog.packages, package_index);
  if (!s_package_rule_add(package, rule.buf))
  {
    snprintf(s_package_catalog.error, sizeof(s_package_catalog.error),
        "Rule '%s' is invalid or already exists in %s.", rule.buf,
        package->name.buf);
    return false;
  }

  s_package_catalog.selected = package_index;
  ldki_editor_package_catalog_open(editor);
  return true;
}

static bool s_package_catalog_name_from_input(char *out, size_t out_size)
{
  XSlice name = x_slice_trim(x_slice(s_package_catalog.package_input));
  int written;

  if (name.length == 0 || name.length >= out_size)
  {
    return false;
  }

  memcpy(out, name.ptr, name.length);
  out[name.length] = 0;

  size_t extension_length = strlen(LDK_PACKAGE_FILE_EXTENSION);
  if (name.length < extension_length ||
      strcmp(out + name.length - extension_length,
          LDK_PACKAGE_FILE_EXTENSION) != 0)
  {
    written = snprintf(out + name.length, out_size - name.length, "%s",
        LDK_PACKAGE_FILE_EXTENSION);
    if (written != (int)extension_length)
    {
      return false;
    }
  }

  return s_package_name_valid(out);
}

static bool s_package_catalog_package_add(void)
{
  char name[X_SMALLSTR_MAX_LENGTH + 1];

  if (!s_package_catalog_name_from_input(name, sizeof(name)))
  {
    s_package_catalog_error_set(
        "Package names may use letters, numbers, '_', '-' and '.', and must "
        "end in .box.");
    return false;
  }

  if (!s_package_definition_add(name))
  {
    s_package_catalog_error_set(
        "Package already exists or could not be added.");
    return false;
  }

  s_package_catalog.selected = x_array_count(s_package_catalog.packages) - 1;
  s_package_catalog.package_input[0] = 0;
  return true;
}

static bool s_package_catalog_package_remove(u32 package_index)
{
  if (!s_package_catalog.packages ||
      package_index >= x_array_count(s_package_catalog.packages))
  {
    return false;
  }

  LDKEditorPackageDefinition *package =
      x_array_get(s_package_catalog.packages, package_index);
  s_package_definition_terminate(package);
  x_array_delete_at(s_package_catalog.packages, package_index);

  u32 count = x_array_count(s_package_catalog.packages);
  if (count == 0)
  {
    s_package_catalog.selected = UINT32_MAX;
  }
  else if (s_package_catalog.selected >= count)
  {
    s_package_catalog.selected = count - 1;
  }

  return true;
}

static bool s_package_catalog_rule_add_input(void)
{
  if (!s_package_catalog.packages ||
      s_package_catalog.selected >= x_array_count(s_package_catalog.packages))
  {
    return false;
  }

  LDKEditorPackageDefinition *package =
      x_array_get(s_package_catalog.packages, s_package_catalog.selected);
  if (!s_package_rule_add(package, s_package_catalog.rule_input))
  {
    s_package_catalog_error_set("Invalid or duplicate package rule.");
    return false;
  }

  s_package_catalog.rule_input[0] = 0;
  return true;
}

static bool s_package_catalog_rule_remove(u32 rule_index)
{
  if (!s_package_catalog.packages ||
      s_package_catalog.selected >= x_array_count(s_package_catalog.packages))
  {
    return false;
  }

  LDKEditorPackageDefinition *package =
      x_array_get(s_package_catalog.packages, s_package_catalog.selected);
  if (rule_index >= x_array_count(package->rules))
  {
    return false;
  }

  x_array_delete_at(package->rules, rule_index);
  return true;
}

static void s_package_catalog_drop(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;

  if (!ui->mouse || !ui->current_window || ui->active_id == 0 ||
      ui->active_window_id == ui->current_window->id ||
      ui->hovered_window_id != ui->current_window->id ||
      !ldk_os_mouse_button_up(
          (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    return;
  }

  u32 type = 0;
  XSmallstr payload = {0};
  if (!ldk_ui_drag_n_drop_payload_get_and_remove(&type, &payload) ||
      type != LDK_EDITOR_DRAG_N_DROP_PAYLOAD_FILE_PATH)
  {
    return;
  }

  XFSPath path = {0};
  x_fs_path_set(&path, payload.buf);
  bool is_directory = x_fs_path_is_directory(&path);
  if (!is_directory && !x_fs_path_is_file(&path))
  {
    s_package_catalog_error_set("Dropped path no longer exists.");
    return;
  }

  ldki_editor_package_catalog_add_path(
      editor, s_package_catalog.selected, &path, is_directory);
}

void ldki_editor_package_catalog_show(LDKEditor *instance, void *data)
{
  enum
  {
    ACTION_NONE,
    ACTION_REMOVE_PACKAGE,
    ACTION_REMOVE_RULE,
  };

  (void)data;
  LDKEditorContext *editor = (LDKEditorContext *)instance;
  LDKUIContext *ui = &editor->ui;

  if (!editor->project.loaded)
  {
    ldk_ui_label(ui, "Open a project to edit its packages.");
    return;
  }

  if (!s_package_catalog_load(editor) || !s_package_catalog.packages)
  {
    ldk_ui_label(ui, "Package catalog is unavailable.");
    if (s_package_catalog.error[0])
    {
      ldk_ui_label(ui, s_package_catalog.error);
    }
    return;
  }

  bool editable = !editor->project_build.active &&
                  editor->editor_state == LDK_EDITOR_STATE_STOPED;
  u32 action = ACTION_NONE;
  u32 action_index = UINT32_MAX;

  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_label(ui,
      "Packages are stored in [.packages] in the project .ldk file.\n"
      "Rules are relative to runtree. Prefix a rule with ! to exclude it.\n"
      "Changes remain a draft until Save.\n"
      "Drop files or folders from Project Explorer to add a rule to the "
      "selected package.");

  ldk_ui_begin_disabled(ui, !editable);
  s_package_catalog.scroll = ldk_ui_begin_scrollview(
      ui, s_package_catalog.scroll,
      LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  ldk_ui_label(ui, "Packages");
  for (u32 i = 0; i < x_array_count(s_package_catalog.packages); ++i)
  {
    LDKEditorPackageDefinition *package =
        x_array_get(s_package_catalog.packages, i);
    ldk_ui_push_id_u32(ui, i);
    ldk_ui_begin_horizontal(ui);
    ldk_ui_set_next_width(ui, ldk_ui_px(24.0f));
    ldk_ui_label(ui, s_package_catalog.selected == i ? ">" : "");
    if (ldk_ui_button_flat(ui, package->name.buf))
    {
      s_package_catalog.selected = i;
    }
    if (ldk_ui_button(ui, "remove"))
    {
      action = ACTION_REMOVE_PACKAGE;
      action_index = i;
    }
    ldk_ui_end_horizontal(ui);
    ldk_ui_pop_id(ui);
  }

  ldk_ui_begin_horizontal(ui);
  u32 package_input_result = ldk_ui_input_box(ui,
      s_package_catalog.package_input,
      (u32)sizeof(s_package_catalog.package_input));
  bool add_package = ldk_ui_button(ui, "+ Add package") ||
                     (package_input_result & LDK_UI_INPUT_BOX_COMMITTED) != 0;
  ldk_ui_end_horizontal(ui);

  ldk_ui_horizontal_line(ui);

  if (s_package_catalog.selected < x_array_count(s_package_catalog.packages))
  {
    LDKEditorPackageDefinition *package = x_array_get(
        s_package_catalog.packages, s_package_catalog.selected);
    ldk_ui_label(ui, package->name.buf);

    for (u32 i = 0; i < x_array_count(package->rules); ++i)
    {
      LDKEditorPackageRule *rule = x_array_get(package->rules, i);
      ldk_ui_push_id_u32(ui, i);
      ldk_ui_begin_horizontal(ui);
      ldk_ui_label(ui, rule->text.buf);
      if (ldk_ui_button(ui, "remove"))
      {
        action = ACTION_REMOVE_RULE;
        action_index = i;
      }
      ldk_ui_end_horizontal(ui);
      ldk_ui_pop_id(ui);
    }

    ldk_ui_begin_horizontal(ui);
    u32 rule_input_result = ldk_ui_input_box(ui, s_package_catalog.rule_input,
        (u32)sizeof(s_package_catalog.rule_input));
    bool add_rule = ldk_ui_button(ui, "+ Add rule") ||
                    (rule_input_result & LDK_UI_INPUT_BOX_COMMITTED) != 0;
    ldk_ui_end_horizontal(ui);

    if (add_rule)
    {
      s_package_catalog_rule_add_input();
    }
  }
  else
  {
    ldk_ui_label(ui, "Add or select a package to edit its rules.");
  }

  if (add_package)
  {
    s_package_catalog_package_add();
  }

  ldk_ui_end_scrollview(ui);

  if (action == ACTION_REMOVE_PACKAGE)
  {
    s_package_catalog_package_remove(action_index);
  }
  else if (action == ACTION_REMOVE_RULE)
  {
    s_package_catalog_rule_remove(action_index);
  }

  if (editable &&
      s_package_catalog.selected < x_array_count(s_package_catalog.packages))
  {
    s_package_catalog_drop(editor);
  }

  if (s_package_catalog.error[0])
  {
    ldk_ui_set_next_weight(ui, 0.0f);
    ldk_ui_label(ui, s_package_catalog.error);
  }

  if (!editable)
  {
    ldk_ui_set_next_weight(ui, 0.0f);
    ldk_ui_label(ui,
        "Package editing is available only in STOP, outside a build.");
  }

  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_horizontal_line(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  ldk_ui_begin_horizontal(ui);
  ldk_ui_spacer(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  bool saved = ldk_ui_button(ui, "Save") &&
               s_package_catalog_manifest_save(editor);
  ldk_ui_end_disabled(ui);

  ldk_ui_set_next_weight(ui, 0.0f);
  bool canceled = ldk_ui_button(ui, "Cancel");
  ldk_ui_end_horizontal(ui);

  if (saved)
  {
    s_package_catalog.error[0] = 0;
    ldki_editor_log_info(editor, "Package catalog saved.");
    s_package_catalog.close_requested = true;
  }
  else if (canceled)
  {
    s_package_catalog.close_requested = true;
  }
}
