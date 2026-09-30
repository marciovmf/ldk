#ifndef LDK_EDITOR_PROJECT_OPTIONS_H
#define LDK_EDITOR_PROJECT_OPTIONS_H

#include <ldk_common.h>
#include <stdbool.h>

typedef struct LDKEditorProjectGenerator
{
  const char *cmake_generator;
  const char *label;
  bool uses_platform;
} LDKEditorProjectGenerator;

u32 ldki_editor_project_generator_count(void);
const LDKEditorProjectGenerator *ldki_editor_project_generator_get(u32 index);
const char *const *ldki_editor_project_generator_labels_get(void);
u32 ldki_editor_project_generator_index_get(const char *generator);

u32 ldki_editor_project_build_type_count(void);
const char *const *ldki_editor_project_build_types_get(void);
u32 ldki_editor_project_build_type_index_get(const char *build_type);

#endif // LDK_EDITOR_PROJECT_OPTIONS_H
