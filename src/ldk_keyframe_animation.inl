/* Built into ldk.c so this first version needs no new build-file entry. */
#include <ldk.h>
#include <ldk_keyframe_animation.h>
#include <float.h>
#include <component/ldk_transform.h>
#include <ldk_property.h>
#include <module/ldk_ecs.h>
#include <module/ldk_asset_source.h>
#include <stdx/stdx_strbuilder.h>
#include <stdx/stdx_tml.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

u64 ldk_keyframe_path_root(void)
{
  return UINT64_C(14695981039346656037);
}

u64 ldk_keyframe_path_child(u64 parent_path, u64 name_hash)
{
  if (!parent_path || !name_hash)
  {
    return 0;
  }
  /* Encode name hashes bytewise in a fixed order for cross-platform paths. */
  for (u32 i = 0; i < 8; i++)
  {
    parent_path ^= (name_hash >> (i * 8)) & 0xffu;
    parent_path *= UINT64_C(1099511628211);
  }
  return parent_path ? parent_path : 1;
}

bool ldk_keyframe_entity_path(
    LDKEntity root, LDKEntity target, u64 *out_path)
{
  LDKEntity cursor = target;
  u64 names[128];
  u32 count = 0;
  u64 hash = ldk_keyframe_path_root();
  if (!out_path)
  {
    return false;
  }
  while (cursor.index != root.index || cursor.version != root.version)
  {
    u64 name_hash = ldk_ecs_entity_name_hash_get(cursor);
    LDKEntity parent;
    if (!name_hash || count >= 128 || x_handle_is_null(cursor))
    {
      return false;
    }
    names[count++] = name_hash;
    parent = ldk_transform_get_parent(cursor);
    if (x_handle_is_null(parent))
    {
      return false;
    }
    cursor = parent;
  }
  if (!ldk_ecs_component_get(root, LDK_COMPONENT_TYPE_TRANSFORM))
  {
    return false;
  }
  while (count)
  {
    hash = ldk_keyframe_path_child(hash, names[--count]);
  }
  *out_path = hash;
  return true;
}

void ldk_keyframe_animation_init(LDKKeyframeAnimation *clip)
{
  if (clip)
  {
    memset(clip, 0, sizeof(*clip));
    clip->duration = 1.0f;
  }
}

void ldk_keyframe_animation_clear(LDKKeyframeAnimation *clip)
{
  if (!clip)
  {
    return;
  }
  for (u32 i = 0; i < clip->track_count; i++)
  {
    free(clip->tracks[i].keys);
  }
  for (u32 i = 0; i < clip->event_count; i++)
  {
    free(clip->events[i].text);
  }
  free(clip->tracks);
  free(clip->events);
  ldk_keyframe_animation_init(clip);
}

static bool s_keyframe_reserve(void **buffer, u32 *capacity,
    u32 required, size_t element_size)
{
  if (required > *capacity)
  {
    u32 next = *capacity ? *capacity : 4;
    while (next < required)
    {
      if (next > UINT32_MAX / 2)
      {
        return false;
      }
      next *= 2;
    }
    if ((size_t)next > SIZE_MAX / element_size)
    {
      return false;
    }
    void *memory = realloc(*buffer, (size_t)next * element_size);
    if (!memory)
    {
      return false;
    }
    *buffer = memory;
    *capacity = next;
  }
  return true;
}

i32 ldk_keyframe_animation_track_find(const LDKKeyframeAnimation *clip,
    u64 path, u32 component_type, const char *property_name)
{
  if (!clip || !property_name)
  {
    return -1;
  }
  for (u32 i = 0; i < clip->track_count; ++i)
  {
    const LDKKeyframeTrack *t = &clip->tracks[i];
    if (t->target_path == path && t->component_type == component_type &&
        strcmp(t->property_name, property_name) == 0)
    {
      return (i32)i;
    }
  }
  return -1;
}

static i32 s_keyframe_track_add(LDKKeyframeAnimation *clip, u64 path,
    u32 component_type, const char *property_name, LDKFieldType value_type)
{
  if (!clip || !path || !component_type || !property_name ||
      !property_name[0] ||
      strlen(property_name) >= LDK_KEYFRAME_PROPERTY_NAME_CAPACITY)
  {
    return -1;
  }
  i32 existing = ldk_keyframe_animation_track_find(
      clip, path, component_type, property_name);
  if (existing >= 0)
  {
    return existing;
  }
  if (clip->track_count >= INT32_MAX ||
      !s_keyframe_reserve((void **)&clip->tracks, &clip->track_capacity,
          clip->track_count + 1, sizeof(*clip->tracks)))
  {
    return -1;
  }
  u32 index = clip->track_count++;
  LDKKeyframeTrack *t = &clip->tracks[index];
  *t = (LDKKeyframeTrack){0};
  t->target_path = path;
  t->component_type = component_type;
  t->value_type = value_type;
  memcpy(t->property_name, property_name, strlen(property_name) + 1);
  return (i32)index;
}

i32 ldk_keyframe_animation_track_add(LDKKeyframeAnimation *clip, u64 path,
    u32 component_type, const char *property_name)
{
  const LDKComponentFieldMeta *field =
      ldk_property_field_find(component_type, property_name);
  if (!ldk_property_field_supported(field))
  {
    return -1;
  }
  return s_keyframe_track_add(clip, path, component_type,
      property_name, field->type);
}

bool ldk_keyframe_animation_track_remove(LDKKeyframeAnimation *clip, u32 track)
{
  if (!clip || track >= clip->track_count)
  {
    return false;
  }
  free(clip->tracks[track].keys);
  memmove(&clip->tracks[track], &clip->tracks[track + 1],
      (clip->track_count - track - 1) * sizeof(*clip->tracks));
  clip->track_count--;
  return true;
}

bool ldk_keyframe_animation_key_set(
    LDKKeyframeAnimation *clip, u32 track, float time, LDKPropertyValue value)
{
  if (!clip || track >= clip->track_count || !isfinite(time) || time < 0.0f ||
      value.type != clip->tracks[track].value_type)
  {
    return false;
  }
  const LDKKeyframeTrack *t = &clip->tracks[track];
  const LDKComponentFieldMeta *field =
      ldk_property_field_find(t->component_type, t->property_name);
  LDKPropertyValue checked;
  /* Validate using the shared property codec. If metadata is temporarily
   * unavailable (game DLL unloaded), clips are not editable. */
  if (!field || !ldk_property_interpolate(field, &value, &value, 0, &checked))
  {
    return false;
  }
  LDKKeyframeTrack *edit = &clip->tracks[track];
  u32 index = 0;
  while (index < edit->count && edit->keys[index].time < time)
  {
    index++;
  }
  if (index < edit->count && edit->keys[index].time == time)
  {
    edit->keys[index].value = value;
    return true;
  }
  if (!s_keyframe_reserve((void **)&edit->keys, &edit->capacity,
          edit->count + 1, sizeof(*edit->keys)))
  {
    return false;
  }
  memmove(&edit->keys[index + 1], &edit->keys[index],
      (edit->count - index) * sizeof(*edit->keys));
  edit->keys[index] = (LDKKeyframe){time, value};
  edit->count++;
  if (time > clip->duration)
  {
    clip->duration = time;
  }
  return true;
}

bool ldk_keyframe_animation_key_remove(
    LDKKeyframeAnimation *clip, u32 track, u32 key)
{
  if (!clip || track >= clip->track_count ||
      key >= clip->tracks[track].count)
  {
    return false;
  }
  LDKKeyframeTrack *t = &clip->tracks[track];
  memmove(&t->keys[key], &t->keys[key + 1],
      (t->count - key - 1) * sizeof(*t->keys));
  t->count--;
  return true;
}

bool ldk_keyframe_animation_sample(const LDKKeyframeAnimation *clip,
    u32 track, float time, LDKPropertyValue *out_value)
{
  if (!clip || track >= clip->track_count || !out_value || !isfinite(time))
  {
    return false;
  }
  const LDKKeyframeTrack *t = &clip->tracks[track];
  if (!t->count)
  {
    return false;
  }
  if (t->count == 1 || time <= t->keys[0].time)
  {
    *out_value = t->keys[0].value;
    return true;
  }
  for (u32 i = 1; i < t->count; i++)
  {
    if (time <= t->keys[i].time)
    {
      const LDKComponentFieldMeta *field =
          ldk_property_field_find(t->component_type, t->property_name);
      if (!field || field->type != t->value_type)
      {
        return false;
      }
      const LDKKeyframe *a = &t->keys[i - 1];
      const LDKKeyframe *b = &t->keys[i];
      float alpha = (time - a->time) / (b->time - a->time);
      return ldk_property_interpolate(field, &a->value, &b->value,
          alpha, out_value);
    }
  }
  *out_value = t->keys[t->count - 1].value;
  return true;
}

bool ldk_keyframe_animation_value_from_entity(LDKEntity entity,
    const LDKKeyframeTrack *track, LDKPropertyValue *out_value)
{
  return track && ldk_property_get(entity, track->component_type,
      track->property_name, out_value);
}

typedef struct LDKKeyframeResolve
{
  u64 path;
  LDKEntity match;
  u32 match_count;
} LDKKeyframeResolve;

static void s_keyframe_resolve_children(LDKEntity entity, u64 parent_path,
    u64 wanted, LDKKeyframeResolve *result, u32 depth)
{
  const LDKTransform *transform;
  LDKEntity child;
  if (depth > 128 || result->match_count > 1)
  {
    return;
  }
  transform = ldk_ecs_component_get_const(entity, LDK_COMPONENT_TYPE_TRANSFORM);
  if (!transform)
  {
    return;
  }
  child = transform->first_child;
  while (!x_handle_is_null(child))
  {
    const LDKTransform *child_transform = ldk_ecs_component_get_const(
        child, LDK_COMPONENT_TYPE_TRANSFORM);
    if (!child_transform)
    {
      break;
    }
    LDKEntity next = child_transform->next_sibling;
    u64 hash = ldk_ecs_entity_name_hash_get(child);
    if (hash)
    {
      u64 path = ldk_keyframe_path_child(parent_path, hash);
      if (path == wanted)
      {
        result->match = child;
        result->match_count++;
      }
      s_keyframe_resolve_children(child, path, wanted, result, depth + 1);
    }
    child = next;
  }
}

bool ldk_keyframe_target_resolve(
    LDKEntity root, u64 path, LDKEntity *out)
{
  if (path == ldk_keyframe_path_root())
  {
    *out = root;
    return true;
  }
  LDKKeyframeResolve result = {0};
  s_keyframe_resolve_children(root, ldk_keyframe_path_root(), path, &result, 0);
  if (result.match_count != 1)
  {
    return false;
  }
  *out = result.match;
  return true;
}

bool ldk_keyframe_animation_apply(
    const LDKKeyframeAnimation *clip, LDKEntity root, float time)
{
  bool ok = true;
  if (!clip || !ldk_ecs_component_get(root, LDK_COMPONENT_TYPE_TRANSFORM))
  {
    return false;
  }
  for (u32 i = 0; i < clip->track_count; ++i)
  {
    const LDKKeyframeTrack *track = &clip->tracks[i];
    LDKPropertyValue value;
    LDKEntity target;
    if (!track->count)
    {
      continue;
    }
    if (!ldk_keyframe_target_resolve(root, track->target_path, &target) ||
        !ldk_keyframe_animation_sample(clip, i, time, &value) ||
        !ldk_property_set(target, track->component_type,
            track->property_name, &value))
    {
      ok = false;
    }
  }
  return ok;
}

static bool s_keyframe_event_add(LDKKeyframeAnimation *clip, float time,
    LDKKeyframeEventValueType type, i32 number, const char *text)
{
  if (!clip || !isfinite(time) || time < 0 ||
      (type == LDK_KEYFRAME_EVENT_STRING && !text))
  {
    return false;
  }
  char *copy = NULL;
  if (text)
  {
    size_t len = strlen(text);
    copy = malloc(len + 1);
    if (!copy)
    {
      return false;
    }
    memcpy(copy, text, len + 1);
  }
  if (!s_keyframe_reserve((void **)&clip->events, &clip->event_capacity,
          clip->event_count + 1, sizeof(*clip->events)))
  {
    free(copy);
    return false;
  }
  u32 index = 0;
  while (index < clip->event_count && clip->events[index].time <= time)
  {
    index++;
  }
  memmove(&clip->events[index + 1], &clip->events[index],
      (clip->event_count - index) * sizeof(*clip->events));
  clip->events[index] = (LDKKeyframeEvent){time, type, number, copy};
  clip->event_count++;
  if (time > clip->duration)
  {
    clip->duration = time;
  }
  return true;
}

bool ldk_keyframe_animation_event_add_integer(
    LDKKeyframeAnimation *clip, float time, i32 number)
{
  return s_keyframe_event_add(clip, time, LDK_KEYFRAME_EVENT_INTEGER,
      number, NULL);
}

bool ldk_keyframe_animation_event_add_string(
    LDKKeyframeAnimation *clip, float time, const char *text)
{
  return s_keyframe_event_add(clip, time, LDK_KEYFRAME_EVENT_STRING,
      0, text);
}

void ldk_keyframe_animation_events_dispatch(
    const LDKKeyframeAnimation *clip, LDKEntity root, float previous_time,
    float current_time, LDKKeyframeEventFn fn, void *user)
{
  if (!clip || !fn || !isfinite(previous_time) || !isfinite(current_time) ||
      current_time < previous_time)
  {
    return;
  }
  for (u32 i = 0; i < clip->event_count; i++)
  {
    const LDKKeyframeEvent *event = &clip->events[i];
    if (event->time > previous_time && event->time <= current_time)
    {
      fn(root, event, user);
    }
  }
}

static void s_keyframe_append_escaped(XStrBuilder *out, const char *s)
{
  x_strbuilder_append_char(out, '"');
  for (; *s; s++)
  {
    switch (*s)
    {
    case '\\': x_strbuilder_append(out, "\\\\"); break;
    case '"': x_strbuilder_append(out, "\\\""); break;
    case '\n': x_strbuilder_append(out, "\\n"); break;
    case '\r': x_strbuilder_append(out, "\\r"); break;
    case '\t': x_strbuilder_append(out, "\\t"); break;
    default: x_strbuilder_append_char(out, *s); break;
    }
  }
  x_strbuilder_append_char(out, '"');
}

bool ldk_keyframe_animation_to_tml(
    const LDKKeyframeAnimation *clip, char **out_source)
{
  if (!clip || !out_source || !isfinite(clip->duration) || clip->duration <= 0)
  {
    return false;
  }
  XStrBuilder *out = x_strbuilder_create();
  if (!out)
  {
    return false;
  }
  x_strbuilder_append_format(out,
      "animation:\n  version: 2\n  duration: %.9f\n  tracks:\n", clip->duration);
  for (u32 i = 0; i < clip->track_count; ++i)
  {
    const LDKKeyframeTrack *t = &clip->tracks[i];
    x_strbuilder_append_format(out,
        "    - target: \"0x%016" PRIx64 "\"\n      component: %u\n      property: ",
        t->target_path, t->component_type);
    s_keyframe_append_escaped(out, t->property_name);
    x_strbuilder_append_format(out, "\n      type: %u\n      keys:\n", (u32)t->value_type);
    for (u32 j = 0; j < t->count; ++j)
    {
      const LDKKeyframe *k = &t->keys[j];
      const LDKPropertyValue *v = &k->value;
      x_strbuilder_append_format(out,
          "        - time: %.9f\n          value: ", k->time);
      switch (t->value_type)
      {
      case LDK_FIELD_FLOAT:
        x_strbuilder_append_format(out, "%.9f", v->vector.x);
        break;
      case LDK_FIELD_VEC2:
      case LDK_FIELD_VEC3:
      case LDK_FIELD_VEC4:
      case LDK_FIELD_QUAT:
      {
        u32 count = t->value_type == LDK_FIELD_VEC2 ? 2 :
            t->value_type == LDK_FIELD_VEC3 ? 3 : 4;
        const float v4[] = {v->vector.x, v->vector.y, v->vector.z, v->vector.w};
        for (u32 axis = 0; axis < count; ++axis)
        {
          x_strbuilder_append_format(out, "%s%.9f",
              axis ? ", " : "", v4[axis]);
        }
        break;
      }
      case LDK_FIELD_BOOL:
        x_strbuilder_append(out, v->integer ? "true" : "false");
        break;
      case LDK_FIELD_STRING:
        s_keyframe_append_escaped(out, v->string.buf);
        break;
      case LDK_FIELD_ENUM:
      case LDK_FIELD_I32:
      case LDK_FIELD_U32:
        x_strbuilder_append_format(out, "%" PRId64, v->integer);
        break;
      default:
        x_strbuilder_destroy(out);
        return false;
      }
      x_strbuilder_append_char(out, '\n');
    }
  }
  x_strbuilder_append(out, "  events:\n");
  for (u32 i = 0; i < clip->event_count; ++i)
  {
    const LDKKeyframeEvent *e = &clip->events[i];
    x_strbuilder_append_format(out, "    - time: %.9f\n", e->time);
    if (e->type == LDK_KEYFRAME_EVENT_INTEGER)
    {
      x_strbuilder_append_format(out, "      integer: %d\n", e->number);
    }
    else
    {
      x_strbuilder_append(out, "      string: ");
      s_keyframe_append_escaped(out, e->text ? e->text : "");
      x_strbuilder_append_char(out, '\n');
    }
  }
  const char *text = x_strbuilder_to_string(out);
  if (!text)
  {
    x_strbuilder_destroy(out);
    return false;
  }
  size_t length = strlen(text);
  char *result = malloc(length + 1);
  if (!result)
  {
    x_strbuilder_destroy(out);
    return false;
  }
  memcpy(result, text, length + 1);
  x_strbuilder_destroy(out);
  *out_source = result;
  return true;
}

static bool s_keyframe_read_hash(const TMLDocument *doc,
    const TMLNode *node, const char *name, u64 *out)
{
  TMLString str;
  char text[32];
  char *end = NULL;
  if (!tml_node_get_string(doc, node, name, &str) ||
      str.size == 0 || str.size >= sizeof(text))
  {
    return false;
  }
  memcpy(text, str.data, str.size);
  text[str.size] = 0;
  unsigned long long value = strtoull(text, &end, 0);
  if (!end || *end || !value)
  {
    return false;
  }
  *out = (u64)value;
  return true;
}

static bool s_keyframe_read_value(const TMLDocument *doc,
    const TMLNode *node, LDKFieldType type, LDKPropertyValue *out)
{
  LDKPropertyValue value = {0};
  value.type = type;
  switch (type)
  {
  case LDK_FIELD_FLOAT:
  {
    f64 number;
    if (!tml_node_get_f64(doc, node, "value", &number) ||
        !isfinite(number) || fabs(number) > FLT_MAX)
      return false;
    value.vector.x = (float)number;
    break;
  }
  case LDK_FIELD_VEC2:
  case LDK_FIELD_VEC3:
  case LDK_FIELD_VEC4:
  case LDK_FIELD_QUAT:
  {
    const TMLEntry *entry = tml_node_find_entry(doc, node, "value");
    TMLF64Slice slice;
    u32 needed = type == LDK_FIELD_VEC2 ? 2 :
        type == LDK_FIELD_VEC3 ? 3 : 4;
    if (!entry || !tml_entry_get_f64_array(doc, entry, &slice) ||
        slice.count != needed)
      return false;
    float *axes[] = {&value.vector.x, &value.vector.y,
        &value.vector.z, &value.vector.w};
    for (u32 i = 0; i < needed; ++i)
    {
      if (!isfinite(slice.data[i]) || fabs(slice.data[i]) > FLT_MAX)
        return false;
      *axes[i] = (float)slice.data[i];
    }
    break;
  }
  case LDK_FIELD_BOOL:
  {
    u8 b;
    if (!tml_node_get_bool(doc, node, "value", &b)) return false;
    value.integer = b ? 1 : 0;
    break;
  }
  case LDK_FIELD_ENUM:
  case LDK_FIELD_I32:
  case LDK_FIELD_U32:
    if (!tml_node_get_i64(doc, node, "value", &value.integer)) return false;
    break;
  case LDK_FIELD_STRING:
  {
    TMLString str;
    if (!tml_node_get_string(doc, node, "value", &str) ||
        str.size > X_SMALLSTR_MAX_LENGTH)
      return false;
    memcpy(value.string.buf, str.data, str.size);
    value.string.buf[str.size] = 0;
    value.string.length = str.size;
    break;
  }
  default:
    return false;
  }
  *out = value;
  return true;
}

bool ldk_keyframe_animation_from_tml(
    LDKKeyframeAnimation *clip, const char *source)
{
  if (!clip || !source)
  {
    return false;
  }
  TMLParseResult parsed = tml_parse(source);
  if (!parsed.ok)
  {
    return false;
  }
  LDKKeyframeAnimation loaded;
  ldk_keyframe_animation_init(&loaded);
  const TMLDocument *doc = parsed.document;
  const TMLNode *root = tml_root_node_at(doc, 0);
  f64 duration = 0;
  i64 version = 0;
  bool valid = root && root->name.size == 9 &&
      memcmp(root->name.data, "animation", 9) == 0 &&
      doc->root_node_count == 1 &&
      tml_node_get_i64(doc, root, "version", &version) && version == 2 &&
      tml_node_get_f64(doc, root, "duration", &duration) &&
      isfinite(duration) && duration > 0.0 && duration <= FLT_MAX;
  if (valid)
  {
    loaded.duration = (float)duration;
    const TMLNode *tracks = tml_node_find_child(doc, root, "tracks");
    if (tracks)
    {
      for (u32 i = 0; valid && i < tracks->child_count; ++i)
      {
        const TMLNode *node = tml_node_child_at(doc, tracks, i);
        u64 path;
        i64 comp, kind;
        TMLString property;
        char name[LDK_KEYFRAME_PROPERTY_NAME_CAPACITY];
        if (!node || !s_keyframe_read_hash(doc, node, "target", &path) ||
            !tml_node_get_i64(doc, node, "component", &comp) ||
            comp <= 0 || comp > UINT32_MAX ||
            !tml_node_get_i64(doc, node, "type", &kind) ||
            kind < LDK_FIELD_BOOL || kind > LDK_FIELD_ASSET_KEYFRAME_ANIMATION ||
            !tml_node_get_string(doc, node, "property", &property) ||
            !property.size || property.size >= sizeof(name))
        {
          valid = false;
          break;
        }
        memcpy(name, property.data, property.size);
        name[property.size] = 0;
        if (ldk_keyframe_animation_track_find(
                &loaded, path, (u32)comp, name) >= 0)
        {
          valid = false;
          break;
        }
        i32 track = s_keyframe_track_add(&loaded,
            path, (u32)comp, name, (LDKFieldType)kind);
        if (track < 0)
        {
          valid = false;
          break;
        }
        const TMLNode *keys = tml_node_find_child(doc, node, "keys");
        for (u32 k = 0; valid && keys && k < keys->child_count; ++k)
        {
          const TMLNode *key = tml_node_child_at(doc, keys, k);
          f64 time;
          LDKPropertyValue value;
          if (!key || !tml_node_get_f64(doc, key, "time", &time) ||
              !isfinite(time) || time < 0 || time > duration ||
              !s_keyframe_read_value(doc, key, (LDKFieldType)kind, &value))
          {
            valid = false;
            break;
          }
          LDKKeyframeTrack *t = &loaded.tracks[track];
          /* Loader may run before game metadata becomes available. */
          if (!s_keyframe_reserve((void **)&t->keys, &t->capacity,
                  t->count + 1, sizeof(*t->keys)) ||
              (t->count && time <= t->keys[t->count - 1].time))
          {
            valid = false;
            break;
          }
          t->keys[t->count++] = (LDKKeyframe){(float)time, value};
        }
      }
    }
    const TMLNode *events = tml_node_find_child(doc, root, "events");
    if (events)
    {
      for (u32 i = 0; valid && i < events->child_count; ++i)
      {
        const TMLNode *node = tml_node_child_at(doc, events, i);
        const TMLEntry *integer = node ? tml_node_find_entry(doc, node, "integer") : NULL;
        const TMLEntry *string = node ? tml_node_find_entry(doc, node, "string") : NULL;
        f64 time;
        i64 number;
        TMLString text;
        if (!node || !tml_node_get_f64(doc, node, "time", &time) ||
            !isfinite(time) || time < 0.0 || time > duration)
        {
          valid = false;
          break;
        }
        if (integer && !string && tml_entry_get_i64(integer, &number) &&
            number >= INT32_MIN && number <= INT32_MAX)
        {
          valid = ldk_keyframe_animation_event_add_integer(
              &loaded, (float)time, (i32)number);
        }
        else if (string && !integer && tml_entry_get_string(string, &text))
        {
          char *terminated = malloc((size_t)text.size + 1u);
          if (!terminated)
          {
            valid = false;
          }
          else
          {
            memcpy(terminated, text.data, text.size);
            terminated[text.size] = 0;
            valid = ldk_keyframe_animation_event_add_string(
                &loaded, (float)time, terminated);
            free(terminated);
          }
        }
        else
        {
          valid = false;
        }
      }
    }
  }
  tml_document_free(parsed.document);
  if (!valid)
  {
    ldk_keyframe_animation_clear(&loaded);
    return false;
  }
  ldk_keyframe_animation_clear(clip);
  *clip = loaded;
  return true;
}

bool ldk_keyframe_animation_load(
    LDKKeyframeAnimation *clip, const char *asset_path)
{
  LDKAssetSource *source = ldk_module_get(LDK_MODULE_ASSET_SOURCE);
  LDKAssetSourceFile file;
  if (!clip || !source || !asset_path ||
      !ldk_asset_source_find(source, asset_path, &file))
  {
    return false;
  }
  u64 size = ldk_asset_source_file_size(&file);
  if (!size || size >= SIZE_MAX)
  {
    return false;
  }
  char *text = malloc((size_t)size + 1);
  if (!text)
  {
    return false;
  }
  bool ok = ldk_asset_source_file_read(&file, text, size);
  if (ok)
  {
    text[size] = 0;
    ok = ldk_keyframe_animation_from_tml(clip, text);
  }
  free(text);
  return ok;
}

bool ldk_keyframe_animation_save(
    const LDKKeyframeAnimation *clip, const char *asset_path)
{
  LDKAssetSource *source = ldk_module_get(LDK_MODULE_ASSET_SOURCE);
  char *text;
  if (!source || !asset_path || !ldk_keyframe_animation_to_tml(clip, &text))
  {
    return false;
  }
  bool ok = ldk_asset_source_file_write(source, asset_path,
      text, strlen(text));
  free(text);
  return ok;
}
