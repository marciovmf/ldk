#include <ldk_common.h>
#include <ldk_project.h>

#ifdef LDK_EDITOR

#include <stdx/stdx_ini.h>
#include <stdx/stdx_filesystem.h>
#include <stdx/stdx_strbuilder.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef LDK_PROJECT_DEFAULT_CMAKE_GENERATOR
#define LDK_PROJECT_DEFAULT_CMAKE_GENERATOR "Visual Studio 18 2026"
#endif

#ifndef LDK_PROJECT_DEFAULT_CONFIG
#define LDK_PROJECT_DEFAULT_CONFIG "Debug"
#endif

static bool s_string_is_empty(const char* str)
{
  return str == NULL || str[0] == 0;
}

static bool s_file_write_text(const XFSPath* path, const char* text)
{
  FILE* file;
  size_t len;

  if (path == NULL || text == NULL)
  {
    return false;
  }

  file = fopen(x_fs_path_cstr(path), "wb");
  if (!file)
  {
    return false;
  }

  len = strlen(text);
  if (fwrite(text, 1, len, file) != len)
  {
    fclose(file);
    return false;
  }

  fclose(file);
  return true;
}

static bool s_builder_write_text(const XFSPath* path, const XStrBuilder* builder)
{
  if (builder == NULL)
  {
    return false;
  }

  return s_file_write_text(path, x_strbuilder_to_string(builder));
}

static void s_project_resolve_path(XFSPath* out_path, const XFSPath* base_path, const char* value)
{
  X_ASSERT(out_path != NULL);
  X_ASSERT(base_path != NULL);

  if (s_string_is_empty(value))
  {
    x_fs_path_set(out_path, "");
    return;
  }

  if (x_fs_path_is_absolute_cstr(value))
  {
    x_fs_path_set(out_path, value);
  }
  else
  {
    x_fs_path(out_path, x_fs_path_cstr(base_path), value);
  }

  x_fs_path_normalize(out_path);
}

static void s_project_set_default_paths(LDKProject* project, const char* config)
{
  const char* build_config;

  X_ASSERT(project != NULL);

  build_config = s_string_is_empty(config) ? LDK_PROJECT_DEFAULT_CONFIG : config;

  x_fs_path(&project->cache_path, x_fs_path_cstr(&project->project_root_path), ".ldk");
  x_fs_path_normalize(&project->cache_path);

  x_fs_path(&project->runtime_ini_path, x_fs_path_cstr(&project->run_root_path), "game.ini");
  x_fs_path_normalize(&project->runtime_ini_path);

  x_fs_path(&project->game_cmake_path, x_fs_path_cstr(&project->project_root_path), "ldk_game.cmake");
  x_fs_path_normalize(&project->game_cmake_path);

  x_fs_path(&project->game_dll_path, x_fs_path_cstr(&project->cache_path), build_config, "game_shared.dll");
  x_fs_path_normalize(&project->game_dll_path);
}

static bool s_line_is_private_section(const char* line, bool* out_is_section)
{
  const char* p;

  X_ASSERT(out_is_section != NULL);

  *out_is_section = false;

  if (line == NULL)
  {
    return false;
  }

  p = line;
  while (*p && isspace((unsigned char)*p))
  {
    p++;
  }

  if (*p != '[')
  {
    return false;
  }

  p++;
  *out_is_section = true;

  while (*p && isspace((unsigned char)*p))
  {
    p++;
  }

  return *p == '.';
}

static bool s_copy_runtime_sections(FILE* in_file, FILE* out_file)
{
  char line[4096];
  bool skip_section;
  bool wrote_any_line;

  skip_section = false;
  wrote_any_line = false;

  while (fgets(line, sizeof(line), in_file))
  {
    bool is_section;
    bool private_section;

    private_section = s_line_is_private_section(line, &is_section);
    if (is_section)
    {
      skip_section = private_section;
    }

    if (!skip_section)
    {
      fputs(line, out_file);
      wrote_any_line = true;
    }
  }

  return wrote_any_line;
}

static bool s_project_create_required_dirs(
    const XFSPath* project_root_path,
    const XFSPath* cache_path,
    const XFSPath* workspace_path,
    const XFSPath* src_path,
    const XFSPath* runtree_path,
    const XFSPath* assets_path)
{
  if (!x_fs_directory_create_recursive(x_fs_path_cstr(project_root_path)))
  {
    return false;
  }

  if (!x_fs_directory_create_recursive(x_fs_path_cstr(cache_path)))
  {
    return false;
  }

  if (!x_fs_directory_create_recursive(x_fs_path_cstr(workspace_path)))
  {
    return false;
  }

  if (!x_fs_directory_create_recursive(x_fs_path_cstr(src_path)))
  {
    return false;
  }

  if (!x_fs_directory_create_recursive(x_fs_path_cstr(runtree_path)))
  {
    return false;
  }

  if (!x_fs_directory_create_recursive(x_fs_path_cstr(assets_path)))
  {
    return false;
  }

  return true;
}

static void s_project_append_project_file_text(
    XStrBuilder* builder,
    const char* project_name,
    const char* cmake_generator)
{
  x_strbuilder_append(builder, "[.project]\n");
  x_strbuilder_append_format(builder, "project_name = \"%s\"\n", project_name);
  x_strbuilder_append(builder, "project_cmake_root = \"workspace\"\n");
  x_strbuilder_append(builder, "project_run_root = \"runtree\"\n");
  x_strbuilder_append_format(builder, "project_cmake_generator = \"%s\"\n", cmake_generator);
  x_strbuilder_append(builder, "\n");
  x_strbuilder_append(builder, "[general]\n");
  x_strbuilder_append(builder, "asset_root = \"assets\"\n");
  x_strbuilder_append(builder, "log_file = \"ldk.log\"\n");
  x_strbuilder_append(builder, "\n");
  x_strbuilder_append(builder, "[display]\n");
  x_strbuilder_append_format(builder, "title = \"%s\"\n", project_name);
  x_strbuilder_append(builder, "width = 1280\n");
  x_strbuilder_append(builder, "height = 720\n");
  x_strbuilder_append(builder, "fullscreen = false\n");
}

static void s_project_append_game_ini_text(XStrBuilder* builder, const char* project_name)
{
  x_strbuilder_append(builder, "[general]\n");
  x_strbuilder_append(builder, "asset_root = \"assets\"\n");
  x_strbuilder_append(builder, "log_file = \"ldk.log\"\n");
  x_strbuilder_append(builder, "\n");
  x_strbuilder_append(builder, "[display]\n");
  x_strbuilder_append_format(builder, "title = \"%s\"\n", project_name);
  x_strbuilder_append(builder, "width = 1280\n");
  x_strbuilder_append(builder, "height = 720\n");
  x_strbuilder_append(builder, "fullscreen = false\n");
}

static void s_project_append_game_cmake_text(XStrBuilder* builder)
{
  x_strbuilder_append(builder, "set(LDK_GAME_SOURCES\n");
  x_strbuilder_append(builder, "  \"${OPTION_GAME_DIR}/src/game.c\"\n");
  x_strbuilder_append(builder, ")\n");
  x_strbuilder_append(builder, "\n");
  x_strbuilder_append(builder, "set(LDK_GAME_INCLUDE_DIRS\n");
  x_strbuilder_append(builder, "  \"${OPTION_GAME_DIR}/src\"\n");
  x_strbuilder_append(builder, ")\n");
  x_strbuilder_append(builder, "\n");
  x_strbuilder_append(builder, "set(LDK_GAME_DEFINITIONS)\n");
  x_strbuilder_append(builder, "\n");
  x_strbuilder_append(builder, "set(LDK_GAME_LIBRARIES)\n");
}

static void s_project_append_game_c_text(XStrBuilder* builder)
{
  x_strbuilder_append(builder, "#include <ldk_game.h>\n");
  x_strbuilder_append(builder, "\n");
  x_strbuilder_append(builder, "bool game_initialize(LDKGame* game)\n");
  x_strbuilder_append(builder, "{\n");
  x_strbuilder_append(builder, "  (void) game;\n");
  x_strbuilder_append(builder, "  return true;\n");
  x_strbuilder_append(builder, "}\n");
  x_strbuilder_append(builder, "\n");
  x_strbuilder_append(builder, "bool game_start(LDKGame* game)\n");
  x_strbuilder_append(builder, "{\n");
  x_strbuilder_append(builder, "  (void) game;\n");
  x_strbuilder_append(builder, "  return true;\n");
  x_strbuilder_append(builder, "}\n");
  x_strbuilder_append(builder, "\n");
  x_strbuilder_append(builder, "void game_update(LDKGame* game, float delta_time)\n");
  x_strbuilder_append(builder, "{\n");
  x_strbuilder_append(builder, "  (void) game;\n");
  x_strbuilder_append(builder, "  (void) delta_time;\n");
  x_strbuilder_append(builder, "}\n");
  x_strbuilder_append(builder, "\n");
  x_strbuilder_append(builder, "void game_stop(LDKGame* game)\n");
  x_strbuilder_append(builder, "{\n");
  x_strbuilder_append(builder, "  (void) game;\n");
  x_strbuilder_append(builder, "}\n");
  x_strbuilder_append(builder, "\n");
  x_strbuilder_append(builder, "void game_terminate(LDKGame* game)\n");
  x_strbuilder_append(builder, "{\n");
  x_strbuilder_append(builder, "  (void) game;\n");
  x_strbuilder_append(builder, "}\n");
}

static bool s_project_create_files(
    const XFSPath* project_file_path,
    const XFSPath* game_ini_path,
    const XFSPath* game_cmake_path,
    const XFSPath* game_c_path,
    const char* project_name,
    const char* cmake_generator)
{
  XStrBuilder* project_text;
  XStrBuilder* game_ini_text;
  XStrBuilder* game_cmake_text;
  XStrBuilder* game_c_text;
  bool result;

  project_text = x_strbuilder_create();
  game_ini_text = x_strbuilder_create();
  game_cmake_text = x_strbuilder_create();
  game_c_text = x_strbuilder_create();

  if (!project_text || !game_ini_text || !game_cmake_text || !game_c_text)
  {
    x_strbuilder_destroy(project_text);
    x_strbuilder_destroy(game_ini_text);
    x_strbuilder_destroy(game_cmake_text);
    x_strbuilder_destroy(game_c_text);
    return false;
  }

  s_project_append_project_file_text(project_text, project_name, cmake_generator);
  s_project_append_game_ini_text(game_ini_text, project_name);
  s_project_append_game_cmake_text(game_cmake_text);
  s_project_append_game_c_text(game_c_text);

  result = s_builder_write_text(project_file_path, project_text)
    && s_builder_write_text(game_ini_path, game_ini_text)
    && s_builder_write_text(game_cmake_path, game_cmake_text);

  if (result && !x_fs_path_is_file(game_c_path))
  {
    result = s_builder_write_text(game_c_path, game_c_text);
  }

  x_strbuilder_destroy(project_text);
  x_strbuilder_destroy(game_ini_text);
  x_strbuilder_destroy(game_cmake_text);
  x_strbuilder_destroy(game_c_text);

  return result;
}

bool ldk_project_create(const LDKProjectCreateDesc* desc)
{
  XFSPath project_root_path;
  XFSPath project_file_path;
  XFSPath cache_path;
  XFSPath workspace_path;
  XFSPath src_path;
  XFSPath runtree_path;
  XFSPath assets_path;
  XFSPath game_ini_path;
  XFSPath game_cmake_path;
  XFSPath game_c_path;
  const char* project_name;
  const char* cmake_generator;

  if (desc == NULL || s_string_is_empty(desc->project_root_path))
  {
    return false;
  }

  project_name = s_string_is_empty(desc->project_name) ? "Game" : desc->project_name;
  cmake_generator = s_string_is_empty(desc->cmake_generator) ? LDK_PROJECT_DEFAULT_CMAKE_GENERATOR : desc->cmake_generator;

  x_fs_path_set(&project_root_path, desc->project_root_path);
  x_fs_path_normalize(&project_root_path);

  x_fs_path(&cache_path, x_fs_path_cstr(&project_root_path), ".ldk");
  x_fs_path(&workspace_path, x_fs_path_cstr(&project_root_path), "workspace");
  x_fs_path(&src_path, x_fs_path_cstr(&project_root_path), "src");
  x_fs_path(&runtree_path, x_fs_path_cstr(&project_root_path), "runtree");
  x_fs_path(&assets_path, x_fs_path_cstr(&runtree_path), "assets");

  x_fs_path_normalize(&cache_path);
  x_fs_path_normalize(&workspace_path);
  x_fs_path_normalize(&src_path);
  x_fs_path_normalize(&runtree_path);
  x_fs_path_normalize(&assets_path);

  if (!s_project_create_required_dirs(
        &project_root_path,
        &cache_path,
        &workspace_path,
        &src_path,
        &runtree_path,
        &assets_path))
  {
    return false;
  }

  x_fs_path(&project_file_path, x_fs_path_cstr(&project_root_path), project_name);
  x_fs_path_change_extension(&project_file_path, "ldk");

  x_fs_path(&game_ini_path, x_fs_path_cstr(&runtree_path), "game.ini");
  x_fs_path(&game_cmake_path, x_fs_path_cstr(&project_root_path), "ldk_game.cmake");
  x_fs_path(&game_c_path, x_fs_path_cstr(&src_path), "game.c");

  x_fs_path_normalize(&project_file_path);
  x_fs_path_normalize(&game_ini_path);
  x_fs_path_normalize(&game_cmake_path);
  x_fs_path_normalize(&game_c_path);

  return s_project_create_files(
      &project_file_path,
      &game_ini_path,
      &game_cmake_path,
      &game_c_path,
      project_name,
      cmake_generator);
}

bool ldk_project_load(LDKProject* project, const char* project_file_path)
{
  XIni ini;
  XIniError ini_error;
  const char* project_name;
  const char* cmake_root;
  const char* run_root;
  const char* cmake_generator;

  if (project == NULL || s_string_is_empty(project_file_path))
  {
    return false;
  }

  memset(project, 0, sizeof(*project));
  memset(&ini, 0, sizeof(ini));
  memset(&ini_error, 0, sizeof(ini_error));

  x_fs_path_set(&project->project_file_path, project_file_path);
  x_fs_path_normalize(&project->project_file_path);

  if (!x_fs_path_is_file(&project->project_file_path))
  {
    return false;
  }

  x_fs_path_dirname(&project->project_file_path, &project->project_root_path);
  x_fs_path_normalize(&project->project_root_path);

  if (!x_ini_load_file(x_fs_path_cstr(&project->project_file_path), &ini, &ini_error))
  {
    return false;
  }

  project_name = x_ini_get(&ini, ".project", "project_name", "Game");
  cmake_root = x_ini_get(&ini, ".project", "project_cmake_root", "workspace");
  run_root = x_ini_get(&ini, ".project", "project_run_root", "runtree");
  cmake_generator = x_ini_get(&ini, ".project", "project_cmake_generator", LDK_PROJECT_DEFAULT_CMAKE_GENERATOR);

  x_smallstr_from_cstr(&project->name, project_name);
  x_smallstr_from_cstr(&project->cmake_generator, cmake_generator);

  s_project_resolve_path(&project->cmake_root_path, &project->project_root_path, cmake_root);
  s_project_resolve_path(&project->run_root_path, &project->project_root_path, run_root);
  s_project_set_default_paths(project, LDK_PROJECT_DEFAULT_CONFIG);

  x_ini_free(&ini);

  if (!x_fs_path_is_file(&project->game_cmake_path))
  {
    memset(project, 0, sizeof(*project));
    return false;
  }

  project->loaded = true;
  return true;
}

void ldk_project_unload(LDKProject* project)
{
  if (project == NULL)
  {
    return;
  }

  memset(project, 0, sizeof(*project));
}

bool ldk_project_write_runtime_ini(const LDKProject* project)
{
  FILE* in_file;
  FILE* out_file;
  bool ok;

  if (project == NULL || !project->loaded)
  {
    return false;
  }

  if (!x_fs_directory_create_recursive(x_fs_path_cstr(&project->run_root_path)))
  {
    return false;
  }

  in_file = fopen(x_fs_path_cstr(&project->project_file_path), "rb");
  if (!in_file)
  {
    return false;
  }

  out_file = fopen(x_fs_path_cstr(&project->runtime_ini_path), "wb");
  if (!out_file)
  {
    fclose(in_file);
    return false;
  }

  ok = s_copy_runtime_sections(in_file, out_file);

  fclose(out_file);
  fclose(in_file);
  return ok;
}

bool ldk_project_build_game_dll(const LDKProject* project, const LDKProjectBuildDesc* desc)
{
  XStrBuilder* configure_command;
  XStrBuilder* build_command;
  const char* config;
  bool result;

  if (project == NULL || desc == NULL || !project->loaded)
  {
    return false;
  }

  if (s_string_is_empty(desc->ldk_source_root_path))
  {
    return false;
  }

  config = s_string_is_empty(desc->config) ? LDK_PROJECT_DEFAULT_CONFIG : desc->config;

  if (!x_fs_directory_create_recursive(x_fs_path_cstr(&project->cmake_root_path)))
  {
    return false;
  }

  configure_command = x_strbuilder_create();
  build_command = x_strbuilder_create();

  if (!configure_command || !build_command)
  {
    x_strbuilder_destroy(configure_command);
    x_strbuilder_destroy(build_command);
    return false;
  }

  x_strbuilder_append_format(configure_command,
      "cmake -S \"%s\" -B \"%s\" -G \"%s\" "
      "-DOPTION_BUILD_GAME=ON "
      "-DOPTION_GAME_DIR=\"%s\"",
      desc->ldk_source_root_path,
      x_fs_path_cstr(&project->cmake_root_path),
      project->cmake_generator.buf,
      x_fs_path_cstr(&project->project_root_path));

  if (desc->use_prebuilt_ldk)
  {
    if (s_string_is_empty(desc->prebuilt_ldk_path))
    {
      x_strbuilder_destroy(configure_command);
      x_strbuilder_destroy(build_command);
      return false;
    }

    x_strbuilder_append_format(configure_command,
        " -DOPTION_LDK_USE_PREBUILT=ON "
        "-DOPTION_LDK_PREBUILT_DIR=\"%s\"",
        desc->prebuilt_ldk_path);
  }

  x_strbuilder_append_format(build_command,
      "cmake --build \"%s\" --config \"%s\" --target game_shared",
      x_fs_path_cstr(&project->cmake_root_path),
      config);

  result = system(x_strbuilder_to_string(configure_command)) == 0
    && system(x_strbuilder_to_string(build_command)) == 0;

  x_strbuilder_destroy(configure_command);
  x_strbuilder_destroy(build_command);

  return result;
}

#endif // LDK_EDITOR
