#include <ldk_common.h>

#define X_IMPL_TIME
#include <stdx/stdx_time.h>

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

typedef struct LDKMetaSystem
{
  char symbol_name[128];
  char name[128];
  char type_name[128];
  char update[128];
  char initialize[128];
  char terminate[128];
  char flags[256];
  char bucket[128];
  char order[64];
  char source_path[1024];
  u64 id;
  u32 first_field;
  u32 field_count;
  bool stateful;
} LDKMetaSystem;

typedef struct LDKMetaState
{
  LDKMetaComponent* components;
  u32 component_count;
  u32 component_capacity;

  LDKMetaField* fields;
  u32 field_count;
  u32 field_capacity;

  LDKMetaSystem *systems;
  u32 system_count;

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

static u64 ldk_meta_hash_fnv1a64(const char *text)
{
  u64 hash = UINT64_C(14695981039346656037);

  while (*text)
  {
    hash ^= (u8)*text;
    hash *= UINT64_C(1099511628211);
    text += 1;
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
    bool field_ended;

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
    field_ended = strchr(line, ';') != NULL;

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

      if (field_ended)
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

static bool ldk_meta_system_name_is_valid(const char *name)
{
  const char *cursor;

  if (!name || (!isalpha((unsigned char)name[0]) && name[0] != '_'))
  {
    return false;
  }

  cursor = name + 1;
  while (*cursor)
  {
    if (!ldk_meta_is_ident_char(*cursor))
    {
      return false;
    }
    ++cursor;
  }

  return true;
}

static bool ldk_meta_system_callbacks_are_valid(
    LDKMetaState *state, const LDKMetaSystem *system)
{
  if (!state || !system)
  {
    return false;
  }

  if ((system->update[0] && !ldk_meta_system_name_is_valid(system->update)) ||
      (system->initialize[0] &&
          !ldk_meta_system_name_is_valid(system->initialize)) ||
      (system->terminate[0] &&
          !ldk_meta_system_name_is_valid(system->terminate)))
  {
    ldk_meta_set_error(
        state, "system callbacks must be C identifiers");
    return false;
  }

  return true;
}

static bool ldk_meta_system_option_copy(char *out, size_t out_size,
    const char *value, const char *error, LDKMetaState *state)
{
  size_t length;

  if (!out || out_size == 0 || !value || !value[0])
  {
    ldk_meta_set_error(state, error);
    return false;
  }

  length = strlen(value);
  if (length >= out_size)
  {
    ldk_meta_set_error(state, error);
    return false;
  }

  memcpy(out, value, length + 1u);
  return true;
}

static bool ldk_meta_parse_system_options(LDKMetaState *state,
    const char *begin, const char *end, LDKMetaSystem *system)
{
  char buffer[1024];
  char *token;
  size_t length;

  if (!state || !begin || !end || !system || end < begin)
  {
    return false;
  }

  length = (size_t)(end - begin);
  if (length >= sizeof(buffer))
  {
    ldk_meta_set_error(state, "system annotation is too long");
    return false;
  }

  memcpy(buffer, begin, length);
  buffer[length] = 0;
  token = strtok(buffer, " \t\r");
  while (token)
  {
    char *equals = strchr(token, '=');
    char *key;
    char *value;

    if (!equals)
    {
      ldk_meta_set_error(state, "expected key=value in //@system annotation");
      return false;
    }

    *equals = 0;
    key = token;
    value = equals + 1;
    if (!value[0])
    {
      ldk_meta_set_error(state, "empty //@system option value");
      return false;
    }

    if (strcmp(key, "name") == 0)
    {
      if (!ldk_meta_system_option_copy(system->name, sizeof(system->name),
              value, "system name is too long", state))
      {
        return false;
      }
    }
    else if (strcmp(key, "update") == 0)
    {
      if (!ldk_meta_system_option_copy(system->update, sizeof(system->update),
              value, "system update callback is too long", state))
      {
        return false;
      }
    }
    else if (strcmp(key, "initialize") == 0 || strcmp(key, "init") == 0)
    {
      if (!ldk_meta_system_option_copy(system->initialize,
              sizeof(system->initialize), value,
              "system initialize callback is too long", state))
      {
        return false;
      }
    }
    else if (strcmp(key, "terminate") == 0)
    {
      if (!ldk_meta_system_option_copy(system->terminate,
              sizeof(system->terminate), value,
              "system terminate callback is too long", state))
      {
        return false;
      }
    }
    else if (strcmp(key, "flags") == 0)
    {
      if (!ldk_meta_system_option_copy(system->flags, sizeof(system->flags),
              value, "system flags expression is too long", state))
      {
        return false;
      }
    }
    else if (strcmp(key, "bucket") == 0)
    {
      if (!ldk_meta_system_option_copy(system->bucket, sizeof(system->bucket),
              value, "system bucket expression is too long", state))
      {
        return false;
      }
    }
    else if (strcmp(key, "order") == 0)
    {
      if (!ldk_meta_system_option_copy(system->order, sizeof(system->order),
              value, "system order expression is too long", state))
      {
        return false;
      }
    }
    else
    {
      ldk_meta_set_error(state, "unknown //@system option");
      return false;
    }

    token = strtok(NULL, " \t\r");
  }

  return true;
}

static bool ldk_meta_push_system(LDKMetaState *state, LDKMetaSystem *system)
{
  LDKMetaSystem *systems;
  const char *identity;

  if (!state || !system)
  {
    return false;
  }

  identity = system->symbol_name;
  system->id = ldk_meta_hash_fnv1a64(identity);
  if (system->id < UINT64_C(0x100))
  {
    ldk_meta_set_error(state, "system hash falls in the reserved ID range");
    return false;
  }

  for (u32 i = 0; i < state->system_count; ++i)
  {
    if (strcmp(state->systems[i].symbol_name, system->symbol_name) == 0 ||
        state->systems[i].id == system->id)
    {
      ldk_meta_set_error(state, "duplicate system name or system hash collision");
      return false;
    }
  }

  systems = (LDKMetaSystem *)realloc(state->systems,
      ((size_t)state->system_count + 1u) * sizeof(*systems));
  if (!systems)
  {
    ldk_meta_set_error(state, "failed to allocate system metadata");
    return false;
  }

  state->systems = systems;
  state->systems[state->system_count++] = *system;
  return true;
}

static bool ldk_meta_path_is_header(const char *path)
{
  const char *dot = path ? strrchr(path, '.') : NULL;
  return dot && (strcmp(dot, ".h") == 0 || strcmp(dot, ".hpp") == 0 ||
                    strcmp(dot, ".hh") == 0);
}

static bool ldk_meta_parse_system_struct(LDKMetaState *state,
    const char *declaration, const char *path, LDKMetaSystem *system)
{
  const char *cursor = declaration;
  const char *open_brace;
  const char *close_brace;
  const char *name_begin;
  const char *name_end;
  u32 first_field;

  if (strncmp(cursor, "typedef", strlen("typedef")) != 0 ||
      ldk_meta_is_ident_char(cursor[strlen("typedef")]))
  {
    return false;
  }
  cursor += strlen("typedef");
  while (isspace((unsigned char)*cursor))
  {
    ++cursor;
  }
  if (strncmp(cursor, "struct", strlen("struct")) != 0 ||
      ldk_meta_is_ident_char(cursor[strlen("struct")]))
  {
    return false;
  }

  if (!ldk_meta_path_is_header(path))
  {
    ldk_meta_set_error(state, "stateful //@system structs must be declared in a header");
    return false;
  }

  open_brace = strchr(cursor, '{');
  if (!open_brace)
  {
    ldk_meta_set_error(state, "expected '{' in system struct");
    return false;
  }
  close_brace = ldk_meta_find_matching_brace(open_brace);
  if (!close_brace)
  {
    ldk_meta_set_error(state, "unclosed system struct");
    return false;
  }

  name_begin = close_brace + 1;
  while (*name_begin && isspace((unsigned char)*name_begin))
  {
    ++name_begin;
  }
  name_end = name_begin;
  while (*name_end && ldk_meta_is_ident_char(*name_end))
  {
    ++name_end;
  }
  if (name_begin == name_end)
  {
    ldk_meta_set_error(state, "expected typedef name after system struct");
    return false;
  }

  ldk_meta_copy_ident(system->type_name, sizeof(system->type_name),
      name_begin, name_end);
  snprintf(system->symbol_name, sizeof(system->symbol_name), "%s",
      system->type_name);
  if (!system->name[0])
  {
    snprintf(system->name, sizeof(system->name), "%s", system->type_name);
  }
  if (!ldk_meta_system_name_is_valid(system->name))
  {
    ldk_meta_set_error(state, "system name must be a C identifier");
    return false;
  }
  if (!ldk_meta_system_callbacks_are_valid(state, system))
  {
    return false;
  }
  snprintf(system->source_path, sizeof(system->source_path), "%s", path);
  for (char *c = system->source_path; *c; ++c)
  {
    if (*c == '\\')
    {
      *c = '/';
    }
    else if (*c == '"')
    {
      ldk_meta_set_error(state, "system header path contains a quote");
      return false;
    }
  }
  system->stateful = true;
  system->first_field = state->field_count;
  first_field = state->field_count;
  if (!ldk_meta_parse_struct_body(
          state, system->type_name, open_brace + 1, close_brace))
  {
    return false;
  }
  system->field_count = state->field_count - first_field;

  if (!system->update[0] && !system->initialize[0] && !system->terminate[0])
  {
    ldk_meta_set_error(state,
        "stateful //@system requires update, initialize/init or terminate");
    return false;
  }

  return true;
}

static bool ldk_meta_parse_system_function(LDKMetaState *state,
    const char *declaration, LDKMetaSystem *system)
{
  const char *open_paren = strchr(declaration, '(');
  const char *name_end;
  const char *name_begin;
  char function_name[128];

  if (!open_paren)
  {
    ldk_meta_set_error(state, "expected function declaration after //@system");
    return false;
  }

  name_end = open_paren;
  while (name_end > declaration && isspace((unsigned char)name_end[-1]))
  {
    --name_end;
  }
  name_begin = name_end;
  while (name_begin > declaration && ldk_meta_is_ident_char(name_begin[-1]))
  {
    --name_begin;
  }
  if (name_begin == name_end ||
      (!isalpha((unsigned char)*name_begin) && *name_begin != '_'))
  {
    ldk_meta_set_error(state, "expected function identifier after //@system");
    return false;
  }

  ldk_meta_copy_ident(function_name, sizeof(function_name), name_begin, name_end);
  if (system->update[0])
  {
    ldk_meta_set_error(state,
        "function //@system uses the annotated function as update; remove update=");
    return false;
  }
  snprintf(system->update, sizeof(system->update), "%s", function_name);
  if (!ldk_meta_system_callbacks_are_valid(state, system))
  {
    return false;
  }

  if (system->name[0])
  {
    if (!ldk_meta_system_name_is_valid(system->name))
    {
      ldk_meta_set_error(state,
          "function //@system name must be a C identifier");
      return false;
    }
    snprintf(system->symbol_name, sizeof(system->symbol_name), "%s",
        system->name);
  }
  else
  {
    snprintf(system->symbol_name, sizeof(system->symbol_name), "%s",
        function_name);
    snprintf(system->name, sizeof(system->name), "%s", function_name);
  }

  system->stateful = false;
  return true;
}

static bool ldk_meta_parse_system_at(
    LDKMetaState *state, const char *marker, const char *path)
{
  const char *annotation_begin = marker + strlen("//@system");
  const char *annotation_end = strchr(annotation_begin, '\n');
  const char *declaration;
  LDKMetaSystem system;

  memset(&system, 0, sizeof(system));
  snprintf(system.flags, sizeof(system.flags), "LDK_SYSTEM_FLAG_ENABLED");
  snprintf(system.bucket, sizeof(system.bucket), "LDK_SYSTEM_BUCKET_UPDATE");
  snprintf(system.order, sizeof(system.order), "0");

  if (!annotation_end)
  {
    annotation_end = annotation_begin + strlen(annotation_begin);
  }
  if (!ldk_meta_parse_system_options(
          state, annotation_begin, annotation_end, &system))
  {
    return false;
  }

  declaration = annotation_end;
  while (*declaration && isspace((unsigned char)*declaration))
  {
    ++declaration;
  }
  if (!*declaration)
  {
    ldk_meta_set_error(state, "missing declaration after //@system");
    return false;
  }

  if (!ldk_meta_parse_system_struct(state, declaration, path, &system))
  {
    if (state->error[0])
    {
      return false;
    }
    if (!ldk_meta_parse_system_function(state, declaration, &system))
    {
      return false;
    }
  }

  return ldk_meta_push_system(state, &system);
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

  cursor = text;
  while ((cursor = strstr(cursor, "//@system")) != NULL)
  {
    if (!ldk_meta_parse_system_at(state, cursor, path))
    {
      free(text);
      return false;
    }
    cursor += strlen("//@system");
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
  fprintf(out, "// Generated by ldk comet tool.\n");
  fprintf(out, "// DO NOT EDIT MANUALLY!\n");
  fprintf(out, "// ----------------------------------------------------------\n\n");
  fprintf(out, "#include <ldk_common.h>\n");
  fprintf(out, "#include <ldk_game.h>\n");
  fprintf(out, "#include <stddef.h>\n");
  fprintf(out, "#include <editor/ldk_component_metadata.h>\n\n");


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
  for (i = 0; i < state->system_count; ++i)
  {
    LDKMetaSystem *system = &state->systems[i];
    const char *initialize = system->initialize[0] ? system->initialize : "NULL";
    const char *update = system->update[0] ? system->update : "NULL";
    const char *terminate = system->terminate[0] ? system->terminate : "NULL";
    fprintf(out, "#define LDK_SYSTEM_%s 0x%016llxULL\n",
        system->symbol_name, (unsigned long long)system->id);
    if (system->stateful)
    {
      fprintf(out,
          "#define LDK_SYSTEM_DESC_%s "
          "(&(const LDKSystemDesc){.id = LDK_SYSTEM_%s, .name = \"%s\", "
          ".flags = %s, .bucket = %s, .order = %s, .data_size = sizeof(%s), "
          ".initialize = %s, .update = %s, .terminate = %s})\n",
          system->symbol_name, system->symbol_name, system->name,
          system->flags, system->bucket, system->order, system->type_name,
          initialize, update, terminate);
    }
    else
    {
      fprintf(out,
          "#define LDK_SYSTEM_DESC_%s "
          "(&(const LDKSystemDesc){.id = LDK_SYSTEM_%s, .name = \"%s\", "
          ".flags = %s, .bucket = %s, .order = %s, .data_size = 0u, "
          ".initialize = %s, .update = %s, .terminate = %s})\n",
          system->symbol_name, system->symbol_name, system->name,
          system->flags, system->bucket, system->order,
          initialize, update, terminate);
    }
  }
  fprintf(out, "\n#define ldk_system_id(T) LDK_SYSTEM_##T\n");
  fprintf(out, "#define ldk_system_desc(T) LDK_SYSTEM_DESC_##T\n\n");
  fprintf(out, "\n#ifdef LDK_COMPONENT_METADATA_IMPLEMENTATION\n\n");

  for (i = 0; i < state->system_count; ++i)
  {
    LDKMetaSystem *system = &state->systems[i];
    bool already_included = false;
    if (!system->stateful)
    {
      continue;
    }
    for (u32 j = 0; j < i; ++j)
    {
      if (state->systems[j].stateful &&
          strcmp(state->systems[j].source_path, system->source_path) == 0)
      {
        already_included = true;
        break;
      }
    }
    if (!already_included)
    {
      fprintf(out, "#include \"%s\"\n", system->source_path);
    }
  }
  if (state->system_count)
  {
    fprintf(out, "\n");
  }

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
          "    { \"%s\", %s, offsetof(%s, %s), %uu, %s, %.9ff, %.9ff },\n",
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

  fprintf(out, "u32 game_component_metadata_count(void)\n");
  fprintf(out, "{\n");
  fprintf(out, "  return %uu;\n", state->component_count);
  fprintf(out, "}\n\n");

  fprintf(out, "const LDKComponentMeta* game_component_metadata_get(u32 index)\n");
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

  for (i = 0; i < state->system_count; ++i)
  {
    LDKMetaSystem *system = &state->systems[i];

    fprintf(out, "static inline const LDKSystemMeta *%s_system_meta(void)\n",
        system->symbol_name);
    fprintf(out, "{\n");
    if (system->stateful && system->field_count)
    {
      fprintf(out, "  static const LDKComponentFieldMeta fields[] =\n");
      fprintf(out, "  {\n");
      for (u32 field_index = 0; field_index < system->field_count; ++field_index)
      {
        LDKMetaField *field =
            &state->fields[system->first_field + field_index];
        u32 emitted_flags = ldk_meta_emit_flags(field->flags);
        float min_value = field->has_min ? field->min_value : 0.0f;
        float max_value = field->has_max ? field->max_value : 0.0f;

        fprintf(out,
            "    { \"%s\", %s, offsetof(%s, %s), %uu, %s, %.9ff, %.9ff },\n",
            field->field_name, field->field_kind, system->type_name,
            field->field_name, emitted_flags, field->widget, min_value,
            max_value);
      }
      fprintf(out, "  };\n\n");
    }

    fprintf(out, "  static const LDKSystemMeta meta =\n");
    fprintf(out, "  {\n");
    fprintf(out, "    \"%s\",\n", system->name);
    fprintf(out, "    ldk_system_id(%s),\n", system->symbol_name);
    if (system->stateful)
    {
      fprintf(out, "    sizeof(%s),\n", system->type_name);
    }
    else
    {
      fprintf(out, "    0u,\n");
    }
    if (system->stateful && system->field_count)
    {
      fprintf(out, "    fields,\n");
    }
    else
    {
      fprintf(out, "    NULL,\n");
    }
    fprintf(out, "    %uu\n", system->field_count);
    fprintf(out, "  };\n\n");
    fprintf(out, "  return &meta;\n");
    fprintf(out, "}\n\n");
  }

  fprintf(out, "u32 game_system_metadata_count(void)\n");
  fprintf(out, "{\n");
  fprintf(out, "  return %uu;\n", state->system_count);
  fprintf(out, "}\n\n");
  fprintf(out, "const LDKSystemMeta *game_system_metadata_get(u32 index)\n");
  fprintf(out, "{\n");
  fprintf(out, "  switch (index)\n");
  fprintf(out, "  {\n");
  for (i = 0; i < state->system_count; ++i)
  {
    fprintf(out, "    case %uu:\n", i);
    fprintf(out, "      return %s_system_meta();\n\n",
        state->systems[i].symbol_name);
  }
  fprintf(out, "    default:\n");
  fprintf(out, "      return NULL;\n");
  fprintf(out, "  }\n");
  fprintf(out, "}\n\n");
  fprintf(out, "#endif // LDK_COMPONENT_METADATA_IMPLEMENTATION \n");
  fprintf(out, "#endif // LDK_COMPONENTS_GENERATED_H\n");

  fclose(out);
  return true;
}

bool ldk_meta_generate_header(const char **input_files, u32 input_file_count,
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

  // print components found
  for (u32 i = 0; i < state.component_count; i++)
  {
    LDKMetaComponent* meta = &state.components[i];
    printf(" Component %d/%d - %s : 0x%X\n",
           (i+1), state.component_count,
           (const char*)&meta->type_name[0],
           state.components[i].type_id);
  }

  for (u32 i = 0; i < state.system_count; ++i)
  {
    printf(" System %u/%u - %s : 0x%016llX\n", i + 1u,
        state.system_count, state.systems[i].name,
        (unsigned long long)state.systems[i].id);
  }

  if (ok && !ldk_meta_write_header(&state, output_header_path))
  {
    fprintf(stderr, "ldk_meta_gen: %s\n", state.error);
    ok = false;
  }

  free(state.components);
  free(state.fields);
  free(state.systems);

  return ok;
}


void show_usage()
{
  printf("usage\ncmg output_file <files>\n");
  printf("<files> is a list of C headers and sources\n");
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
  XTimer timer;

  x_timer_start(&timer);
  bool success = ldk_meta_generate_header(files, num_files, argv[1]);
  XTime elapsed = x_timer_elapsed(&timer);
  double milliseconds = x_time_milliseconds(elapsed);

  if (success)
  {
    printf("Component/system metadata extraction finished in %f milliseconds\n", milliseconds);
    return 0;
  }

  fprintf(stderr, "Metadata extraction failed.\n");
  return 1;
}

