#include "ldk_editor_project_options.h"
#include <stddef.h>
#include <string.h>

static const LDKEditorProjectGenerator s_editor_project_generators[] = {
    {"Visual Studio 18 2026", "Visual Studio 2026", true},
    {"Visual Studio 17 2022", "Visual Studio 2022", true},
    {"Ninja", "Ninja", false},
    {"Ninja Multi-Config", "Ninja Multi-Config", false},
    {"NMake Makefiles", "NMake", false},
    {"MinGW Makefiles", "MinGW Make", false},
};

static const char *const s_editor_project_generator_labels[] = {
    "Visual Studio 2026",
    "Visual Studio 2022",
    "Ninja",
    "Ninja Multi-Config",
    "NMake",
    "MinGW Make",
};

static const char *const s_editor_project_build_types[] = {
    "Debug",
    "Release",
    "RelWithDebInfo",
};

u32 ldki_editor_project_generator_count(void)
{
  return (u32)(sizeof(s_editor_project_generators) /
               sizeof(s_editor_project_generators[0]));
}

const LDKEditorProjectGenerator *ldki_editor_project_generator_get(u32 index)
{
  if (index >= ldki_editor_project_generator_count())
  {
    return NULL;
  }

  return &s_editor_project_generators[index];
}

const char *const *ldki_editor_project_generator_labels_get(void)
{
  return s_editor_project_generator_labels;
}

u32 ldki_editor_project_generator_index_get(const char *generator)
{
  if (generator != NULL)
  {
    for (u32 i = 0; i < ldki_editor_project_generator_count(); ++i)
    {
      if (strcmp(generator, s_editor_project_generators[i].cmake_generator) == 0)
      {
        return i;
      }
    }
  }

  return 0;
}

u32 ldki_editor_project_build_type_count(void)
{
  return (u32)(sizeof(s_editor_project_build_types) /
               sizeof(s_editor_project_build_types[0]));
}

const char *const *ldki_editor_project_build_types_get(void)
{
  return s_editor_project_build_types;
}

u32 ldki_editor_project_build_type_index_get(const char *build_type)
{
  if (build_type != NULL)
  {
    for (u32 i = 0; i < ldki_editor_project_build_type_count(); ++i)
    {
      if (strcmp(build_type, s_editor_project_build_types[i]) == 0)
      {
        return i;
      }
    }
  }

  return 0;
}
