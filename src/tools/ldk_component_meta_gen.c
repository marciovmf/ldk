#include <ldk_common.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef uint32_t u32;
typedef uint64_t u64;

typedef struct LDKMetaField
{
  char component_name[128];
  char type_name[128];
  char field_name[128];
  char field_kind[64];
  char widget[64];
  char label[128];
  u32 flags;
  float min_value;
  float max_value;
  bool has_min;
  bool has_max;
} LDKMetaField;

typedef struct LDKMetaComponent
{
  char type_name[128];
  char meta_fn_name[160];
  u32 type_id;
  u32 first_field;
  u32 field_count;
} LDKMetaComponent;

typedef struct LDKMetaState
{
  LDKMetaComponent* components;
  u32 component_count;
  u32 component_capacity;

  LDKMetaField* fields;
  u32 field_count;
  u32 field_capacity;

  char error[512];
} LDKMetaState;

#define LDK_META_FLAG_READONLY 1u
#define LDK_META_FLAG_RUNTIME  2u
#define LDK_META_FLAG_HIDDEN   4u

static u32 ldk_meta_hash_fnv1a32(const char* text)
{
  u32 hash = 2166136261u;

  while (*text)
  {
    hash ^= (u8)*text;
    hash *= 16777619u;
    text += 1;
  }

  if (hash == 0u)
  {
    hash = 1u;
  }

  return hash;
}

static void ldk_meta_set_error(LDKMetaState* state, const char* msg)
{
  if (!state || !msg)
  {
    return;
  }

  snprintf(state->error, sizeof(state->error), "%s", msg);
}

static bool ldk_meta_read_file(const char* path, char** out_text, size_t* out_size)
{
  FILE* file = NULL;
  long size = 0;
  char* text = NULL;
  size_t read_size = 0;

  if (!path || !out_text || !out_size)
  {
    return false;
  }

  file = fopen(path, "rb");
  if (!file)
  {
    return false;
  }

  fseek(file, 0, SEEK_END);
  size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (size < 0)
  {
    fclose(file);
    return false;
  }

  text = (char*)malloc((size_t)size + 1u);
  if (!text)
  {
    fclose(file);
    return false;
  }

  read_size = fread(text, 1u, (size_t)size, file);
  fclose(file);

  text[read_size] = 0;

  *out_text = text;
  *out_size = read_size;
  return true;
}

static char* ldk_meta_trim(char* text)
{
  char* end = NULL;

  while (*text && isspace((unsigned char)*text))
  {
    text += 1;
  }

  end = text + strlen(text);
  while (end > text && isspace((unsigned char)end[-1]))
  {
    end -= 1;
  }

  *end = 0;
  return text;
}

static bool ldk_meta_is_ident_char(char c)
{
  return isalnum((unsigned char)c) || c == '_';
}

static void ldk_meta_copy_ident(char* dst, size_t dst_size, const char* begin, const char* end)
{
  size_t size = 0;

  if (!dst || dst_size == 0)
  {
    return;
  }

  size = (size_t)(end - begin);
  if (size >= dst_size)
  {
    size = dst_size - 1u;
  }

  memcpy(dst, begin, size);
  dst[size] = 0;
}

static bool ldk_meta_push_component(LDKMetaState* state, const LDKMetaComponent* component)
{
  LDKMetaComponent* new_components = NULL;
  u32 new_capacity = 0;

  if (state->component_count == state->component_capacity)
  {
    new_capacity = state->component_capacity == 0u ? 16u : state->component_capacity * 2u;
    new_components = (LDKMetaComponent*)realloc(
        state->components,
        sizeof(LDKMetaComponent) * new_capacity);

    if (!new_components)
    {
      return false;
    }

    state->components = new_components;
    state->component_capacity = new_capacity;
  }

  state->components[state->component_count] = *component;
  state->component_count += 1u;
  return true;
}

static bool ldk_meta_push_field(LDKMetaState* state, const LDKMetaField* field)
{
  LDKMetaField* new_fields = NULL;
  u32 new_capacity = 0;

  if (state->field_count == state->field_capacity)
  {
    new_capacity = state->field_capacity == 0u ? 64u : state->field_capacity * 2u;
    new_fields = (LDKMetaField*)realloc(
        state->fields,
        sizeof(LDKMetaField) * new_capacity);

    if (!new_fields)
    {
      return false;
    }

    state->fields = new_fields;
    state->field_capacity = new_capacity;
  }

  state->fields[state->field_count] = *field;
  state->field_count += 1u;
  return true;
}

static bool ldk_meta_kind_from_type(const char* type_name, char* out_kind, size_t out_size, char* out_widget, size_t out_widget_size)
{
  if (strcmp(type_name, "bool") == 0)
  {
    snprintf(out_kind, out_size, "LDK_FIELD_BOOL");
    snprintf(out_widget, out_widget_size, "LDK_FIELD_WIDGET_CHECKBOX");
    return true;
  }

  if (strcmp(type_name, "i32") == 0 || strcmp(type_name, "int") == 0)
  {
    snprintf(out_kind, out_size, "LDK_FIELD_I32");
    snprintf(out_widget, out_widget_size, "LDK_FIELD_WIDGET_I32");
    return true;
  }

  if (strcmp(type_name, "u32") == 0)
  {
    snprintf(out_kind, out_size, "LDK_FIELD_U32");
    snprintf(out_widget, out_widget_size, "LDK_FIELD_WIDGET_U32");
    return true;
  }

  if (strcmp(type_name, "float") == 0)
  {
    snprintf(out_kind, out_size, "LDK_FIELD_FLOAT");
    snprintf(out_widget, out_widget_size, "LDK_FIELD_WIDGET_FLOAT");
    return true;
  }

  if (strcmp(type_name, "Vec2") == 0)
  {
    snprintf(out_kind, out_size, "LDK_FIELD_VEC2");
    snprintf(out_widget, out_widget_size, "LDK_FIELD_WIDGET_VEC2");
    return true;
  }

  if (strcmp(type_name, "Vec3") == 0)
  {
    snprintf(out_kind, out_size, "LDK_FIELD_VEC3");
    snprintf(out_widget, out_widget_size, "LDK_FIELD_WIDGET_VEC3");
    return true;
  }

  if (strcmp(type_name, "Vec4") == 0)
  {
    snprintf(out_kind, out_size, "LDK_FIELD_VEC4");
    snprintf(out_widget, out_widget_size, "LDK_FIELD_WIDGET_VEC4");
    return true;
  }

  if (strcmp(type_name, "Quat") == 0)
  {
    snprintf(out_kind, out_size, "LDK_FIELD_QUAT");
    snprintf(out_widget, out_widget_size, "LDK_FIELD_WIDGET_QUAT");
    return true;
  }

  if (strcmp(type_name, "Mat4") == 0)
  {
    snprintf(out_kind, out_size, "LDK_FIELD_MAT4");
    snprintf(out_widget, out_widget_size, "LDK_FIELD_WIDGET_MAT4");
    return true;
  }

  if (strcmp(type_name, "LDKEntity") == 0)
  {
    snprintf(out_kind, out_size, "LDK_FIELD_ENTITY");
    snprintf(out_widget, out_widget_size, "LDK_FIELD_WIDGET_ENTITY");
    return true;
  }

  if (strcmp(type_name, "LDKAssetMesh") == 0)
  {
    snprintf(out_kind, out_size, "LDK_FIELD_ASSET_MESH");
    snprintf(out_widget, out_widget_size, "LDK_FIELD_WIDGET_ASSET_MESH");
    return true;
  }

  if (strcmp(type_name, "LDKResourceMesh") == 0)
  {
    snprintf(out_kind, out_size, "LDK_FIELD_RESOURCE_MESH");
    snprintf(out_widget, out_widget_size, "LDK_FIELD_WIDGET_RESOURCE_MESH");
    return true;
  }

  return false;
}

static void ldk_meta_apply_inspect_annotation(LDKMetaField* field, const char* annotation)
{
  char buffer[512];
  char* token = NULL;

  if (!field || !annotation)
  {
    return;
  }

  snprintf(buffer, sizeof(buffer), "%s", annotation);
  token = strtok(buffer, " \t\r\n");

  while (token)
  {
    if (strcmp(token, "readonly") == 0)
    {
      field->flags |= LDK_META_FLAG_READONLY;
    }
    else if (strcmp(token, "runtime") == 0)
    {
      field->flags |= LDK_META_FLAG_RUNTIME;
    }
    else if (strcmp(token, "hidden") == 0)
    {
      field->flags |= LDK_META_FLAG_HIDDEN;
    }
    else if (strcmp(token, "slider") == 0)
    {
      snprintf(field->widget, sizeof(field->widget), "LDK_FIELD_WIDGET_SLIDER");
    }
    else if (strcmp(token, "enum") == 0)
    {
      snprintf(field->field_kind, sizeof(field->field_kind), "LDK_FIELD_ENUM");
      snprintf(field->widget, sizeof(field->widget), "LDK_FIELD_WIDGET_ENUM");
    }
    else if (strncmp(token, "min=", 4) == 0)
    {
      field->min_value = (float)atof(token + 4);
      field->has_min = true;
    }
    else if (strncmp(token, "max=", 4) == 0)
    {
      field->max_value = (float)atof(token + 4);
      field->has_max = true;
    }
    else if (strncmp(token, "widget=", 7) == 0)
    {
      snprintf(field->widget, sizeof(field->widget), "LDK_FIELD_WIDGET_%s", token + 7);
    }

    token = strtok(NULL, " \t\r\n");
  }
}

static bool ldk_meta_parse_field_line(
    LDKMetaState* state,
    const char* component_name,
    char* line,
    const char* pending_annotation)
{
  LDKMetaField field;
  char* semi = NULL;
  char* text = NULL;
  char* comment = NULL;
  char* last_space = NULL;
  char* name = NULL;
  char* type = NULL;

  memset(&field, 0, sizeof(field));

  comment = strstr(line, "//");
  if (comment)
  {
    *comment = 0;
  }

  text = ldk_meta_trim(line);
  if (*text == 0)
  {
    return true;
  }

  semi = strchr(text, ';');
  if (!semi)
  {
    return true;
  }

  *semi = 0;

  if (strchr(text, '*') || strchr(text, '[') || strchr(text, '('))
  {
    return true;
  }

  last_space = strrchr(text, ' ');
  if (!last_space)
  {
    return true;
  }

  *last_space = 0;
  type = ldk_meta_trim(text);
  name = ldk_meta_trim(last_space + 1);

  if (*type == 0 || *name == 0)
  {
    return true;
  }

  snprintf(field.component_name, sizeof(field.component_name), "%s", component_name);
  snprintf(field.type_name, sizeof(field.type_name), "%s", type);
  snprintf(field.field_name, sizeof(field.field_name), "%s", name);
  snprintf(field.label, sizeof(field.label), "%s", name);

  if (!ldk_meta_kind_from_type(type, field.field_kind, sizeof(field.field_kind), field.widget, sizeof(field.widget)))
  {
    if (pending_annotation && strstr(pending_annotation, "enum"))
    {
      snprintf(field.field_kind, sizeof(field.field_kind), "LDK_FIELD_ENUM");
      snprintf(field.widget, sizeof(field.widget), "LDK_FIELD_WIDGET_ENUM");
    }
    else
    {
      return true;
    }
  }

  if (pending_annotation)
  {
    ldk_meta_apply_inspect_annotation(&field, pending_annotation);
  }

  if ((field.flags & LDK_META_FLAG_HIDDEN) != 0u)
  {
    return true;
  }

  if (!ldk_meta_push_field(state, &field))
  {
    ldk_meta_set_error(state, "failed to push metadata field");
    return false;
  }

  return true;
}

static bool ldk_meta_parse_struct_body(
    LDKMetaState* state,
    const char* component_name,
    const char* body_begin,
    const char* body_end)
{
  char pending_annotation[512];
  const char* cursor = body_begin;

  pending_annotation[0] = 0;

  while (cursor < body_end)
  {
    char line[1024];
    char* inspect = NULL;
    const char* next = cursor;
    size_t len = 0;

    while (next < body_end && *next != '\n')
    {
      next += 1;
    }

    len = (size_t)(next - cursor);
    if (len >= sizeof(line))
    {
      len = sizeof(line) - 1u;
    }

    memcpy(line, cursor, len);
    line[len] = 0;

    inspect = strstr(line, "//@inspect");
    if (inspect)
    {
      snprintf(pending_annotation, sizeof(pending_annotation), "%s", inspect + strlen("//@inspect"));
    }
    else
    {
      if (!ldk_meta_parse_field_line(state, component_name, line, pending_annotation[0] ? pending_annotation : NULL))
      {
        return false;
      }

      if (strchr(line, ';'))
      {
        pending_annotation[0] = 0;
      }
    }

    cursor = next;
    if (cursor < body_end && *cursor == '\n')
    {
      cursor += 1;
    }
  }

  return true;
}

static const char* ldk_meta_find_matching_brace(const char* open_brace)
{
  const char* p = open_brace;
  int depth = 0;

  while (*p)
  {
    if (*p == '{')
    {
      depth += 1;
    }
    else if (*p == '}')
    {
      depth -= 1;
      if (depth == 0)
      {
        return p;
      }
    }

    p += 1;
  }

  return NULL;
}

static bool ldk_meta_parse_component_at(LDKMetaState* state, const char* marker)
{
  const char* typedef_struct = NULL;
  const char* open_brace = NULL;
  const char* close_brace = NULL;
  const char* name_begin = NULL;
  const char* name_end = NULL;
  LDKMetaComponent component;
  u32 first_field = 0;

  memset(&component, 0, sizeof(component));

  typedef_struct = strstr(marker, "typedef");
  if (!typedef_struct)
  {
    ldk_meta_set_error(state, "expected typedef after //@component");
    return false;
  }

  typedef_struct = strstr(typedef_struct, "struct");
  if (!typedef_struct)
  {
    ldk_meta_set_error(state, "expected struct after //@component");
    return false;
  }

  open_brace = strchr(typedef_struct, '{');
  if (!open_brace)
  {
    ldk_meta_set_error(state, "expected '{' in component struct");
    return false;
  }

  close_brace = ldk_meta_find_matching_brace(open_brace);
  if (!close_brace)
  {
    ldk_meta_set_error(state, "unclosed component struct");
    return false;
  }

  name_begin = close_brace + 1;
  while (*name_begin && isspace((unsigned char)*name_begin))
  {
    name_begin += 1;
  }

  name_end = name_begin;
  while (*name_end && ldk_meta_is_ident_char(*name_end))
  {
    name_end += 1;
  }

  if (name_begin == name_end)
  {
    ldk_meta_set_error(state, "expected typedef name after component struct");
    return false;
  }

  ldk_meta_copy_ident(component.type_name, sizeof(component.type_name), name_begin, name_end);
  snprintf(component.meta_fn_name, sizeof(component.meta_fn_name), "%s_component_meta", component.type_name);
  component.type_id = ldk_meta_hash_fnv1a32(component.type_name);
  component.first_field = state->field_count;

  first_field = state->field_count;
  if (!ldk_meta_parse_struct_body(state, component.type_name, open_brace + 1, close_brace))
  {
    return false;
  }

  component.field_count = state->field_count - first_field;

  if (!ldk_meta_push_component(state, &component))
  {
    ldk_meta_set_error(state, "failed to push metadata component");
    return false;
  }

  return true;
}

static bool ldk_meta_parse_file(LDKMetaState* state, const char* path)
{
  char* text = NULL;
  size_t size = 0;
  const char* cursor = NULL;

  if (!ldk_meta_read_file(path, &text, &size))
  {
    ldk_meta_set_error(state, "failed to read input file");
    return false;
  }

  cursor = text;
  while (true)
  {
    const char* marker = strstr(cursor, "//@component");

    if (!marker)
    {
      break;
    }

    if (!ldk_meta_parse_component_at(state, marker))
    {
      free(text);
      return false;
    }

    cursor = marker + strlen("//@component");
  }

  free(text);
  return true;
}

static u32 ldk_meta_emit_flags(u32 flags)
{
  u32 result = 0u;

  if ((flags & LDK_META_FLAG_READONLY) != 0u)
  {
    result |= 1u;
  }

  if ((flags & LDK_META_FLAG_RUNTIME) != 0u)
  {
    result |= 2u;
  }

  return result;
}

static bool ldk_meta_check_collisions(LDKMetaState* state)
{
  u32 i = 0;

  for (i = 0; i < state->component_count; i++)
  {
    u32 j = i + 1u;

    while (j < state->component_count)
    {
      if (state->components[i].type_id == state->components[j].type_id)
      {
        snprintf(
            state->error,
            sizeof(state->error),
            "component type hash collision: %s and %s",
            state->components[i].type_name,
            state->components[j].type_name);
        return false;
      }

      j += 1u;
    }
  }

  return true;
}

static bool ldk_meta_write_header(LDKMetaState* state, const char* output_path)
{
  FILE* out = NULL;
  u32 i = 0;

  out = fopen(output_path, "wb");
  if (!out)
  {
    ldk_meta_set_error(state, "failed to open output header");
    return false;
  }

  fprintf(out, "#ifndef LDK_COMPONENTS_GENERATED_H\n");
  fprintf(out, "#define LDK_COMPONENTS_GENERATED_H\n");
  fprintf(out, "//----------------------------------------------------------\n");
  fprintf(out, "// Generated by ldk cmg tool.\n");
  fprintf(out, "// DO NOT EDIT MANUALLY!\n");
  fprintf(out, "// ----------------------------------------------------------\n\n");
  fprintf(out, "#include <ldk_common.h>\n");
  fprintf(out, "#include <stddef.h>\n");
  fprintf(out, "#include <editor/ldk_component_meta.h>\n\n");


  for (i = 0; i < state->component_count; i++)
  {
    LDKMetaComponent* component = &state->components[i];

    fprintf(
        out,
        "#define LDK_COMPONENT_%s 0x%08Xu\n",
        component->type_name,
        component->type_id);
  }

  fprintf(out, "\n#define ldk_component_type(T) LDK_COMPONENT_##T\n\n");

  for (i = 0; i < state->component_count; i++)
  {
    LDKMetaComponent* component = &state->components[i];
    u32 field_index = 0;

    fprintf(out, "static inline const LDKComponentMeta* %s(void)\n", component->meta_fn_name);
    fprintf(out, "{\n");
    fprintf(out, "  static const LDKComponentFieldMeta fields[] =\n");
    fprintf(out, "  {\n");

    for (field_index = 0; field_index < component->field_count; field_index++)
    {
      LDKMetaField* field = &state->fields[component->first_field + field_index];
      u32 emitted_flags = ldk_meta_emit_flags(field->flags);
      float min_value = field->has_min ? field->min_value : 0.0f;
      float max_value = field->has_max ? field->max_value : 0.0f;

      fprintf(
          out,
          "    { \"%s\", %s, offsetof(%s, %s), %uu, %s, %.9gf, %.9gf },\n",
          field->field_name,
          field->field_kind,
          component->type_name,
          field->field_name,
          emitted_flags,
          field->widget,
          min_value,
          max_value);
    }

    fprintf(out, "  };\n\n");
    fprintf(out, "  static const LDKComponentMeta meta =\n");
    fprintf(out, "  {\n");
    fprintf(out, "    \"%s\",\n", component->type_name);
    fprintf(out, "    ldk_component_type(%s),\n", component->type_name);
    fprintf(out, "    sizeof(%s),\n", component->type_name);
    fprintf(out, "    fields,\n");
    fprintf(out, "    %uu\n", component->field_count);
    fprintf(out, "  };\n\n");
    fprintf(out, "  return &meta;\n");
    fprintf(out, "}\n\n");
  }

  fprintf(out, "static inline u32 ldk_component_meta_generated_count(void)\n");
  fprintf(out, "{\n");
  fprintf(out, "  return %uu;\n", state->component_count);
  fprintf(out, "}\n\n");

  fprintf(out, "static inline const LDKComponentMeta* ldk_component_meta_generated_get(u32 index)\n");
  fprintf(out, "{\n");
  fprintf(out, "  switch (index)\n");
  fprintf(out, "  {\n");

  for (i = 0; i < state->component_count; i++)
  {
    LDKMetaComponent* component = &state->components[i];

    fprintf(out, "    case %uu:\n", i);
    fprintf(out, "      return %s();\n\n", component->meta_fn_name);
  }

  fprintf(out, "    default:\n");
  fprintf(out, "      return NULL;\n");
  fprintf(out, "  }\n");
  fprintf(out, "}\n\n");

  fprintf(out, "#endif\n");

  fclose(out);
  return true;
}

bool ldk_meta_generate_header(
    const char** input_files,
    u32 input_file_count,
    const char* output_header_path)
{
  LDKMetaState state;
  u32 i = 0;
  bool ok = true;

  memset(&state, 0, sizeof(state));

  if (!input_files || input_file_count == 0u || !output_header_path)
  {
    return false;
  }

  for (i = 0; i < input_file_count; i++)
  {
    if (!ldk_meta_parse_file(&state, input_files[i]))
    {
      fprintf(stderr, "ldk_meta_gen: %s: %s\n", input_files[i], state.error);
      ok = false;
      break;
    }
  }

  if (ok && !ldk_meta_check_collisions(&state))
  {
    fprintf(stderr, "ldk_meta_gen: %s\n", state.error);
    ok = false;
  }

  if (ok && !ldk_meta_write_header(&state, output_header_path))
  {
    fprintf(stderr, "ldk_meta_gen: %s\n", state.error);
    ok = false;
  }

  free(state.components);
  free(state.fields);

  return ok;
}


void show_usage()
{
  printf("usage\ncmg output_file <files>\n");
  printf("<files> is a list of .h files");
}

int main(i32 argc, const char** argv)
{
  if (argc < 3)
  {
    show_usage();
    return 1;
  }

  const u32 num_files = argc - 2;
  const char** files = &argv[2];
  return ldk_meta_generate_header(files, num_files, argv[1]);
}

