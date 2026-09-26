#include "../ldk_profiler_format.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LDK_EDITOR_WINDOW_PROFILER ((LDKEditorWindowId)0x4C444B0Cu)

#ifndef LDK_EDITOR_PROFILER_CAPTURE_CAPACITY
#define LDK_EDITOR_PROFILER_CAPTURE_CAPACITY 128u
#endif

typedef struct LDKEditorProfilerCapture
{
  XFSPath path;
  time_t modified;
} LDKEditorProfilerCapture;

typedef struct LDKEditorProfilerFrame
{
  u64 index;
  u64 begin_ticks;
  u64 end_ticks;
} LDKEditorProfilerFrame;

typedef struct LDKEditorProfilerTreeNode
{
  u32 sample_index;
  i32 parent;
  u32 first_child;
  u32 child_count;
  double total_ms;
  double self_ms;
  bool expanded;
} LDKEditorProfilerTreeNode;

typedef struct LDKEditorProfilerState
{
  LDKEditorProfilerCapture captures[LDK_EDITOR_PROFILER_CAPTURE_CAPACITY];
  u32 capture_count;

  LDKProfilerCaptureHeader header;
  LDKProfilerSourceRecord sources[LDK_PROFILER_SOURCE_CAPACITY];
  bool source_valid[LDK_PROFILER_SOURCE_CAPACITY];
  LDKProfilerThreadRecord threads[LDK_PROFILER_THREAD_CAPACITY];
  bool thread_valid[LDK_PROFILER_THREAD_CAPACITY];
  LDKProfilerCounterRecord counters[LDK_PROFILER_COUNTER_CAPACITY];
  bool counter_valid[LDK_PROFILER_COUNTER_CAPACITY];

  LDKEditorProfilerFrame *frames;
  u32 frame_count;
  u32 frame_capacity;

  LDKProfilerSampleRecord *samples;
  u32 sample_count;
  u32 sample_capacity;

  LDKProfilerCounterSampleRecord *counter_samples;
  u32 counter_sample_count;
  u32 counter_sample_capacity;

  LDKEditorProfilerTreeNode *tree;
  u32 tree_count;
  u32 tree_capacity;

  XFSPath loaded_path;
  XFSPath catalog_root;
  u32 selected_frame;
  i32 selected_tree_node;
  u32 dropped_records;
  LDKUIPoint scroll;
  LDKUIPoint overview_scroll;
  float first_visible_frame;
  bool reveal_selected_frame;
  float timeline_zoom;
  float timeline_offset;
  LDKUIRect timeline_view_rect;
  LDKUIRect timeline_clip_rect;
  float overview_ratio;
  bool splitter_dragging;
  char error[256];
  bool captures_dirty;
  bool window_was_open;
  bool auto_load_pending;
  u64 observed_revision;
  XFSPath automatic_path;
} LDKEditorProfilerState;

static LDKEditorProfilerState s_editor_profiler = {0};

static void s_editor_profiler_error(const char *message)
{
  snprintf(s_editor_profiler.error, sizeof(s_editor_profiler.error), "%s",
      message ? message : "Profiler error.");
}

static bool s_editor_profiler_reserve(
    void **data, u32 *capacity, u32 required, size_t element_size)
{
  void *new_data;
  u32 new_capacity;

  if (required <= *capacity)
  {
    return true;
  }

  new_capacity = *capacity ? *capacity : 256u;
  while (new_capacity < required)
  {
    if (new_capacity > 0x7FFFFFFFu / 2u)
    {
      return false;
    }
    new_capacity *= 2u;
  }

  new_data = realloc(*data, (size_t)new_capacity * element_size);
  if (!new_data)
  {
    return false;
  }

  *data = new_data;
  *capacity = new_capacity;
  return true;
}

static int s_editor_profiler_frame_compare(const void *a, const void *b)
{
  const LDKEditorProfilerFrame *left = (const LDKEditorProfilerFrame *)a;
  const LDKEditorProfilerFrame *right = (const LDKEditorProfilerFrame *)b;
  if (left->index < right->index)
    return -1;
  if (left->index > right->index)
    return 1;
  return 0;
}

static int s_editor_profiler_sample_compare(const void *a, const void *b)
{
  const LDKProfilerSampleRecord *left =
      (const LDKProfilerSampleRecord *)a;
  const LDKProfilerSampleRecord *right =
      (const LDKProfilerSampleRecord *)b;

  if (left->frame_index < right->frame_index)
    return -1;
  if (left->frame_index > right->frame_index)
    return 1;
  if (left->thread_id < right->thread_id)
    return -1;
  if (left->thread_id > right->thread_id)
    return 1;
  if (left->begin_ticks < right->begin_ticks)
    return -1;
  if (left->begin_ticks > right->begin_ticks)
    return 1;
  if (left->end_ticks > right->end_ticks)
    return -1;
  if (left->end_ticks < right->end_ticks)
    return 1;
  return 0;
}

static int s_editor_profiler_counter_sample_compare(
    const void *a, const void *b)
{
  const LDKProfilerCounterSampleRecord *left =
      (const LDKProfilerCounterSampleRecord *)a;
  const LDKProfilerCounterSampleRecord *right =
      (const LDKProfilerCounterSampleRecord *)b;
  if (left->frame_index < right->frame_index)
    return -1;
  if (left->frame_index > right->frame_index)
    return 1;
  if (left->ticks < right->ticks)
    return -1;
  if (left->ticks > right->ticks)
    return 1;
  return 0;
}

static void s_editor_profiler_capture_clear(void)
{
  memset(&s_editor_profiler.header, 0, sizeof(s_editor_profiler.header));
  memset(s_editor_profiler.source_valid, 0,
      sizeof(s_editor_profiler.source_valid));
  memset(s_editor_profiler.thread_valid, 0,
      sizeof(s_editor_profiler.thread_valid));
  memset(s_editor_profiler.counter_valid, 0,
      sizeof(s_editor_profiler.counter_valid));
  s_editor_profiler.frame_count = 0;
  s_editor_profiler.sample_count = 0;
  s_editor_profiler.counter_sample_count = 0;
  s_editor_profiler.tree_count = 0;
  s_editor_profiler.selected_frame = 0;
  s_editor_profiler.selected_tree_node = -1;
  s_editor_profiler.dropped_records = 0;
  s_editor_profiler.first_visible_frame = 0.0f;
  s_editor_profiler.timeline_zoom = 1.0f;
  s_editor_profiler.timeline_offset = 0.0f;
  s_editor_profiler.timeline_view_rect = (LDKUIRect){0};
  s_editor_profiler.timeline_clip_rect = (LDKUIRect){0};
  s_editor_profiler.loaded_path.length = 0;
  s_editor_profiler.loaded_path.buf[0] = 0;
  s_editor_profiler.error[0] = 0;
}

static LDKEditorProfilerFrame *s_editor_profiler_frame_find(u64 index)
{
  for (u32 i = 0; i < s_editor_profiler.frame_count; ++i)
  {
    if (s_editor_profiler.frames[i].index == index)
    {
      return &s_editor_profiler.frames[i];
    }
  }
  return NULL;
}

static bool s_editor_profiler_capture_load(const char *path)
{
  FILE *file;
  LDKProfilerCaptureHeader header;

  if (!path || !path[0])
  {
    return false;
  }

  file = fopen(path, "rb");
  if (!file)
  {
    s_editor_profiler_error("Failed to open profiler capture.");
    return false;
  }

  s_editor_profiler_capture_clear();

  if (fread(&header, 1, sizeof(header), file) != sizeof(header) ||
      header.magic != LDK_PROFILER_CAPTURE_MAGIC ||
      header.version != LDK_PROFILER_CAPTURE_VERSION ||
      header.tick_frequency == 0)
  {
    fclose(file);
    s_editor_profiler_error("Unsupported or invalid profiler capture.");
    return false;
  }

  s_editor_profiler.header = header;

  for (;;)
  {
    LDKProfilerRecordHeader record_header;
    u8 payload[sizeof(LDKProfilerSourceRecord)];

    if (fread(&record_header, 1, sizeof(record_header), file) == 0)
    {
      break;
    }

    if (record_header.size > sizeof(payload) ||
        fread(payload, 1, record_header.size, file) != record_header.size)
    {
      fclose(file);
      s_editor_profiler_error("Profiler capture is truncated.");
      return false;
    }

    switch ((LDKProfilerRecordType)record_header.type)
    {
    case LDK_PROFILER_RECORD_SOURCE:
      if (record_header.size == sizeof(LDKProfilerSourceRecord))
      {
        const LDKProfilerSourceRecord *record =
            (const LDKProfilerSourceRecord *)payload;
        if (record->id > 0 && record->id <= LDK_PROFILER_SOURCE_CAPACITY)
        {
          s_editor_profiler.sources[record->id - 1u] = *record;
          s_editor_profiler.source_valid[record->id - 1u] = true;
        }
      }
      break;

    case LDK_PROFILER_RECORD_THREAD:
      if (record_header.size == sizeof(LDKProfilerThreadRecord))
      {
        const LDKProfilerThreadRecord *record =
            (const LDKProfilerThreadRecord *)payload;
        if (record->id > 0 && record->id <= LDK_PROFILER_THREAD_CAPACITY)
        {
          s_editor_profiler.threads[record->id - 1u] = *record;
          s_editor_profiler.thread_valid[record->id - 1u] = true;
        }
      }
      break;

    case LDK_PROFILER_RECORD_FRAME_BEGIN:
      if (record_header.size == sizeof(LDKProfilerFrameRecord))
      {
        const LDKProfilerFrameRecord *record =
            (const LDKProfilerFrameRecord *)payload;
        if (!s_editor_profiler_reserve((void **)&s_editor_profiler.frames,
                &s_editor_profiler.frame_capacity,
                s_editor_profiler.frame_count + 1u,
                sizeof(LDKEditorProfilerFrame)))
        {
          fclose(file);
          s_editor_profiler_error("Out of memory loading profiler frames.");
          return false;
        }

        s_editor_profiler.frames[s_editor_profiler.frame_count++] =
            (LDKEditorProfilerFrame){
                .index = record->frame_index,
                .begin_ticks = record->ticks,
                .end_ticks = 0};
      }
      break;

    case LDK_PROFILER_RECORD_FRAME_END:
      if (record_header.size == sizeof(LDKProfilerFrameRecord))
      {
        const LDKProfilerFrameRecord *record =
            (const LDKProfilerFrameRecord *)payload;
        LDKEditorProfilerFrame *frame =
            s_editor_profiler_frame_find(record->frame_index);
        if (frame)
        {
          frame->end_ticks = record->ticks;
        }
      }
      break;

    case LDK_PROFILER_RECORD_SAMPLE:
      if (record_header.size == sizeof(LDKProfilerSampleRecord))
      {
        if (!s_editor_profiler_reserve((void **)&s_editor_profiler.samples,
                &s_editor_profiler.sample_capacity,
                s_editor_profiler.sample_count + 1u,
                sizeof(LDKProfilerSampleRecord)))
        {
          fclose(file);
          s_editor_profiler_error("Out of memory loading profiler samples.");
          return false;
        }
        s_editor_profiler.samples[s_editor_profiler.sample_count++] =
            *(const LDKProfilerSampleRecord *)payload;
      }
      break;

    case LDK_PROFILER_RECORD_COUNTER:
      if (record_header.size == sizeof(LDKProfilerCounterRecord))
      {
        const LDKProfilerCounterRecord *record =
            (const LDKProfilerCounterRecord *)payload;
        if (record->id > 0 && record->id <= LDK_PROFILER_COUNTER_CAPACITY)
        {
          s_editor_profiler.counters[record->id - 1u] = *record;
          s_editor_profiler.counter_valid[record->id - 1u] = true;
        }
      }
      break;

    case LDK_PROFILER_RECORD_COUNTER_SAMPLE:
      if (record_header.size == sizeof(LDKProfilerCounterSampleRecord))
      {
        if (!s_editor_profiler_reserve(
                (void **)&s_editor_profiler.counter_samples,
                &s_editor_profiler.counter_sample_capacity,
                s_editor_profiler.counter_sample_count + 1u,
                sizeof(LDKProfilerCounterSampleRecord)))
        {
          fclose(file);
          s_editor_profiler_error("Out of memory loading profiler counters.");
          return false;
        }
        s_editor_profiler
            .counter_samples[s_editor_profiler.counter_sample_count++] =
            *(const LDKProfilerCounterSampleRecord *)payload;
      }
      break;

    case LDK_PROFILER_RECORD_DROPPED:
      if (record_header.size == sizeof(LDKProfilerDroppedRecord))
      {
        s_editor_profiler.dropped_records +=
            ((const LDKProfilerDroppedRecord *)payload)->count;
      }
      break;

    default:
      break;
    }
  }

  fclose(file);

  if (s_editor_profiler.frame_count)
  {
    qsort(s_editor_profiler.frames, s_editor_profiler.frame_count,
        sizeof(*s_editor_profiler.frames), s_editor_profiler_frame_compare);
  }

  if (s_editor_profiler.sample_count)
  {
    qsort(s_editor_profiler.samples, s_editor_profiler.sample_count,
        sizeof(*s_editor_profiler.samples), s_editor_profiler_sample_compare);
  }

  if (s_editor_profiler.counter_sample_count)
  {
    qsort(s_editor_profiler.counter_samples,
        s_editor_profiler.counter_sample_count,
        sizeof(*s_editor_profiler.counter_samples),
        s_editor_profiler_counter_sample_compare);
  }

  x_fs_path_set(&s_editor_profiler.loaded_path, path);
  x_fs_path_normalize(&s_editor_profiler.loaded_path);
  if (s_editor_profiler.frame_count)
  {
    s_editor_profiler.selected_frame =
        s_editor_profiler.frame_count - 1u;
  }
  return true;
}

static int s_editor_profiler_capture_compare(const void *a, const void *b)
{
  const LDKEditorProfilerCapture *left =
      (const LDKEditorProfilerCapture *)a;
  const LDKEditorProfilerCapture *right =
      (const LDKEditorProfilerCapture *)b;
  if (left->modified > right->modified)
    return -1;
  if (left->modified < right->modified)
    return 1;
  return strcmp(left->path.buf, right->path.buf);
}

static void s_editor_profiler_captures_refresh(LDKEditorContext *editor)
{
  XFSPath directory = {0};
  XFSDireEntry entry = {0};
  XFSDireHandle *handle;

  s_editor_profiler.capture_count = 0;
  s_editor_profiler.captures_dirty = false;
  s_editor_profiler.catalog_root.length = 0;
  s_editor_profiler.catalog_root.buf[0] = 0;

  XFSPath path = {0};
  if (!ldki_editor_profiler_path_get(editor, &path) ||
      !x_fs_path_dirname(&path, &directory))
  {
    return;
  }

  x_fs_path_set(
      &s_editor_profiler.catalog_root, editor->project.game_dll_path.buf);
  x_fs_path_normalize(&s_editor_profiler.catalog_root);

  handle = x_fs_find_first_file(directory.buf, &entry);
  if (!handle)
  {
    return;
  }

  do
  {
    size_t length = strlen(entry.name);
    if (entry.is_directory || length < 5 ||
        strcmp(entry.name + length - 5, ".ldkp") != 0 ||
        s_editor_profiler.capture_count >=
            LDK_EDITOR_PROFILER_CAPTURE_CAPACITY)
    {
      continue;
    }

    LDKEditorProfilerCapture *capture =
        &s_editor_profiler.captures[s_editor_profiler.capture_count++];
    x_fs_path(&capture->path, directory.buf, entry.name);
    x_fs_path_normalize(&capture->path);
    capture->modified = entry.last_modified;
  } while (x_fs_find_next_file(handle, &entry));

  x_fs_find_close(handle);

  qsort(s_editor_profiler.captures, s_editor_profiler.capture_count,
      sizeof(s_editor_profiler.captures[0]),
      s_editor_profiler_capture_compare);
}

static double s_editor_profiler_ticks_ms(u64 begin, u64 end)
{
  if (end <= begin || !s_editor_profiler.header.tick_frequency)
  {
    return 0.0;
  }

  return ((double)(end - begin) * 1000.0) /
         (double)s_editor_profiler.header.tick_frequency;
}

static const LDKProfilerSourceRecord *s_editor_profiler_source_get(u32 id)
{
  if (id == 0 || id > LDK_PROFILER_SOURCE_CAPACITY ||
      !s_editor_profiler.source_valid[id - 1u])
  {
    return NULL;
  }
  return &s_editor_profiler.sources[id - 1u];
}

static const char *s_editor_profiler_kind_name(u8 kind)
{
  switch ((LDKProfilerZoneKind)kind)
  {
  case LDK_PROFILER_ZONE_USER:
    return "User";
  case LDK_PROFILER_ZONE_FRAME:
    return "Frame";
  case LDK_PROFILER_ZONE_PHASE:
    return "Phase";
  case LDK_PROFILER_ZONE_BUCKET:
    return "Bucket";
  case LDK_PROFILER_ZONE_SYSTEM:
    return "System";
  default:
    return "Unknown";
  }
}

static bool s_editor_profiler_tree_build(u64 frame_index)
{
  u32 depth_stack[LDK_PROFILER_STACK_CAPACITY];
  u32 stack_count = 0;

  s_editor_profiler.tree_count = 0;
  s_editor_profiler.selected_tree_node = -1;

  for (u32 sample_i = 0; sample_i < s_editor_profiler.sample_count; ++sample_i)
  {
    const LDKProfilerSampleRecord *sample =
        &s_editor_profiler.samples[sample_i];

    if (sample->frame_index != frame_index)
    {
      continue;
    }

    if (!s_editor_profiler_reserve((void **)&s_editor_profiler.tree,
            &s_editor_profiler.tree_capacity,
            s_editor_profiler.tree_count + 1u,
            sizeof(LDKEditorProfilerTreeNode)))
    {
      s_editor_profiler_error("Out of memory building profiler call tree.");
      return false;
    }

    while (stack_count > sample->depth)
    {
      stack_count -= 1u;
    }

    LDKEditorProfilerTreeNode *node =
        &s_editor_profiler.tree[s_editor_profiler.tree_count];
    memset(node, 0, sizeof(*node));
    node->sample_index = sample_i;
    node->parent =
        sample->depth > 0 && stack_count ? (i32)depth_stack[stack_count - 1u]
                                         : -1;
    node->total_ms =
        s_editor_profiler_ticks_ms(sample->begin_ticks, sample->end_ticks);
    node->self_ms = node->total_ms;
    node->expanded = sample->depth < 3u;

    if (node->parent >= 0)
    {
      LDKEditorProfilerTreeNode *parent =
          &s_editor_profiler.tree[node->parent];
      if (parent->child_count == 0)
      {
        parent->first_child = s_editor_profiler.tree_count;
      }
      parent->child_count += 1u;
      parent->self_ms -= node->total_ms;
      if (parent->self_ms < 0.0)
      {
        parent->self_ms = 0.0;
      }
    }

    if (sample->depth < LDK_PROFILER_STACK_CAPACITY)
    {
      while (stack_count <= sample->depth)
      {
        depth_stack[stack_count++] = s_editor_profiler.tree_count;
      }
      depth_stack[sample->depth] = s_editor_profiler.tree_count;
      stack_count = sample->depth + 1u;
    }

    s_editor_profiler.tree_count += 1u;
  }

  return true;
}

static bool s_editor_profiler_node_visible(u32 node_index)
{
  i32 parent = s_editor_profiler.tree[node_index].parent;
  while (parent >= 0)
  {
    if (!s_editor_profiler.tree[parent].expanded)
    {
      return false;
    }
    parent = s_editor_profiler.tree[parent].parent;
  }
  return true;
}

static void s_editor_profiler_frame_select(u32 frame_index)
{
  if (frame_index >= s_editor_profiler.frame_count)
  {
    return;
  }

  if (s_editor_profiler.selected_frame != frame_index)
  {
    s_editor_profiler.timeline_zoom = 1.0f;
    s_editor_profiler.timeline_offset = 0.0f;
  }
  s_editor_profiler.reveal_selected_frame = true;
  s_editor_profiler.selected_frame = frame_index;
  s_editor_profiler_tree_build(
      s_editor_profiler.frames[frame_index].index);
}

static void s_editor_profiler_frame_graph(LDKUIContext *ui)
{
  LDKUIRect graph_rect;
  double max_ms = 0.0;
  u32 first;
  u32 count;
  float bar_width;

  if (!s_editor_profiler.frame_count)
  {
    ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
    ldk_ui_label(ui, "No frames in capture.");
    return;
  }

  count = s_editor_profiler.frame_count;
  if (count > 120u)
  {
    count = 120u;
  }
  if (s_editor_profiler.reveal_selected_frame)
  {
    u32 selected = s_editor_profiler.selected_frame;
    if (selected < (u32)s_editor_profiler.first_visible_frame)
    {
      s_editor_profiler.first_visible_frame = (float)selected;
    }
    else if (selected >= (u32)s_editor_profiler.first_visible_frame + count)
    {
      s_editor_profiler.first_visible_frame = (float)(selected - count + 1u);
    }
    s_editor_profiler.reveal_selected_frame = false;
  }
  ldk_ui_set_next_height(ui, ldk_ui_px(14.0f));
  ldk_ui_spacer(ui);
  s_editor_profiler.first_visible_frame = ldk_ui_widget_scrollbar_horizontal(ui,
      0x50524653u, s_editor_profiler.first_visible_frame, (float)count,
      (float)s_editor_profiler.frame_count, ldk_ui_last_rect(ui));
  first = (u32)s_editor_profiler.first_visible_frame;
  if (first > s_editor_profiler.frame_count - count)
  {
    first = s_editor_profiler.frame_count - count;
  }
  char range[128];
  snprintf(range, sizeof(range), "Frames %llu - %llu (%u total)",
      (unsigned long long)s_editor_profiler.frames[first].index,
      (unsigned long long)s_editor_profiler.frames[first + count - 1u].index,
      s_editor_profiler.frame_count);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, range);

  for (u32 i = first; i < first + count; ++i)
  {
    double ms = s_editor_profiler_ticks_ms(
        s_editor_profiler.frames[i].begin_ticks,
        s_editor_profiler.frames[i].end_ticks);
    if (ms > max_ms)
    {
      max_ms = ms;
    }
  }
  if (max_ms < 1.0)
  {
    max_ms = 1.0;
  }

  ldk_ui_set_next_height(ui, ldk_ui_px(90.0f));
  ldk_ui_spacer(ui);
  graph_rect = ldk_ui_last_rect(ui);

  bar_width = count ? graph_rect.w / (float)count : graph_rect.w;
  if (bar_width < 2.0f)
  {
    bar_width = 2.0f;
  }

  for (u32 i = 0; i < count; ++i)
  {
    u32 frame_i = first + i;
    double ms = s_editor_profiler_ticks_ms(
        s_editor_profiler.frames[frame_i].begin_ticks,
        s_editor_profiler.frames[frame_i].end_ticks);
    float height = (float)(ms / max_ms) * (graph_rect.h - 4.0f);
    LDKUIRect bar = {
        graph_rect.x + (float)i * bar_width,
        graph_rect.y + graph_rect.h - height,
        bar_width > 2.0f ? bar_width - 1.0f : bar_width,
        height};
    rgba32 color = frame_i == s_editor_profiler.selected_frame
                       ? LDK_RGBA32(0xF2C94CFF)
                       : LDK_RGBA32(0x6F7D8CFF);

    if (ldk_ui_widget_color_view(
            ui, 0x50524700u + i, color, bar))
    {
      s_editor_profiler_frame_select(frame_i);
    }
  }
}


static rgba32 s_editor_profiler_kind_color(u8 kind)
{
  switch ((LDKProfilerZoneKind)kind)
  {
  case LDK_PROFILER_ZONE_FRAME:
    return LDK_RGBA32(0x4C566AFF);
  case LDK_PROFILER_ZONE_PHASE:
    return LDK_RGBA32(0x5E81ACFF);
  case LDK_PROFILER_ZONE_BUCKET:
    return LDK_RGBA32(0x81A1C1FF);
  case LDK_PROFILER_ZONE_SYSTEM:
    return LDK_RGBA32(0xA3BE8CFF);
  case LDK_PROFILER_ZONE_USER:
  default:
    return LDK_RGBA32(0xD08770FF);
  }
}

static void s_editor_profiler_timeline_zoom(float zoom, float anchor)
{
  float old_zoom = s_editor_profiler.timeline_zoom;
  if (old_zoom < 1.0f)
  {
    old_zoom = 1.0f;
  }
  if (zoom < 1.0f)
  {
    zoom = 1.0f;
  }
  if (zoom > 1024.0f)
  {
    zoom = 1024.0f;
  }
  float time = s_editor_profiler.timeline_offset + anchor / old_zoom;
  float offset = time - anchor / zoom;
  float maximum = 1.0f - 1.0f / zoom;
  if (offset < 0.0f)
  {
    offset = 0.0f;
  }
  if (offset > maximum)
  {
    offset = maximum;
  }
  s_editor_profiler.timeline_zoom = zoom;
  s_editor_profiler.timeline_offset = offset;
}

/* Process before the enclosing scrollview so one wheel gesture cannot also
 * move the overview vertically. Rectangles are from the previous UI frame. */
static bool s_editor_profiler_timeline_wheel(LDKUIContext *ui)
{
  if (!ui->mouse || !ui->current_window ||
      ui->hovered_window_id != ui->current_window->id)
  {
    return false;
  }
  LDKUIRect rect = s_editor_profiler.timeline_view_rect;
  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
  i32 wheel = ldk_os_mouse_wheel_delta((LDKMouseState *)ui->mouse);
  if (!wheel || rect.w <= 0.0f ||
      !ldk_rectf_contains(&rect, (float)cursor.x, (float)cursor.y) ||
      !ldk_rectf_contains(&s_editor_profiler.timeline_clip_rect,
          (float)cursor.x, (float)cursor.y))
  {
    return false;
  }
  float steps = (float)wheel / 120.0f;
  if (steps > 8.0f)
  {
    steps = 8.0f;
  }
  if (steps < -8.0f)
  {
    steps = -8.0f;
  }
  s_editor_profiler_timeline_zoom(
      s_editor_profiler.timeline_zoom * powf(1.25f, steps),
      ((float)cursor.x - rect.x) / rect.w);
  return true;
}

static void s_editor_profiler_timeline(LDKUIContext *ui)
{
  LDKUIRect timeline_rect;
  const LDKEditorProfilerFrame *frame;
  double frame_ticks;
  const float row_height = 16.0f;

  if (s_editor_profiler.selected_frame >= s_editor_profiler.frame_count)
  {
    return;
  }

  frame = &s_editor_profiler.frames[s_editor_profiler.selected_frame];
  if (frame->end_ticks <= frame->begin_ticks)
  {
    return;
  }

  frame_ticks = (double)(frame->end_ticks - frame->begin_ticks);
  s_editor_profiler_timeline_zoom(s_editor_profiler.timeline_zoom, 0.5f);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "-"))
  {
    s_editor_profiler_timeline_zoom(
        s_editor_profiler.timeline_zoom / 1.5f, 0.5f);
  }
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "+"))
  {
    s_editor_profiler_timeline_zoom(
        s_editor_profiler.timeline_zoom * 1.5f, 0.5f);
  }
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "Fit Frame"))
  {
    s_editor_profiler_timeline_zoom(1.0f, 0.5f);
  }
  char label[128];
  snprintf(label, sizeof(label), "%.2fx | Wheel: zoom at cursor",
      s_editor_profiler.timeline_zoom);
  ldk_ui_label(ui, label);
  ldk_ui_end_horizontal(ui);

  ldk_ui_set_next_height(ui, ldk_ui_px(14.0f));
  ldk_ui_spacer(ui);
  s_editor_profiler.timeline_offset = ldk_ui_widget_scrollbar_horizontal(ui,
      0x50525453u, s_editor_profiler.timeline_offset,
      1.0f / s_editor_profiler.timeline_zoom, 1.0f, ldk_ui_last_rect(ui));
  float view_begin = s_editor_profiler.timeline_offset;
  float view_span = 1.0f / s_editor_profiler.timeline_zoom;
  double frame_ms =
      s_editor_profiler_ticks_ms(frame->begin_ticks, frame->end_ticks);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_spacer(ui);
  LDKUIRect ruler = ldk_ui_last_rect(ui);
  for (u32 tick = 0; tick < 4u; ++tick)
  {
    LDKUIRect cell = ruler;
    cell.w = ruler.w / 4.0f;
    cell.x += cell.w * (float)tick;
    snprintf(label, sizeof(label), "%.3f ms",
        frame_ms * (view_begin + view_span * (float)tick / 4.0f));
    ldk_ui_widget_label(ui, 0x50525200u + tick, label, cell);
  }
  ldk_ui_set_next_height(ui, ldk_ui_px(128.0f));
  ldk_ui_spacer(ui);
  timeline_rect = ldk_ui_last_rect(ui);
  s_editor_profiler.timeline_view_rect = timeline_rect;
  s_editor_profiler.timeline_clip_rect =
      ldk_rectf_intersect(&ui->clip_rect, &timeline_rect);
  LDKUIRect previous_clip = ui->clip_rect;
  ui->clip_rect = s_editor_profiler.timeline_clip_rect;

  for (u32 i = 0; i < s_editor_profiler.sample_count; ++i)
  {
    const LDKProfilerSampleRecord *sample = &s_editor_profiler.samples[i];
    double begin_normalized;
    double end_normalized;
    float y;
    LDKUIRect rect;

    if (sample->frame_index != frame->index ||
        sample->end_ticks <= sample->begin_ticks)
    {
      continue;
    }

    begin_normalized =
        ((double)sample->begin_ticks - (double)frame->begin_ticks) /
        frame_ticks;
    end_normalized =
        ((double)sample->end_ticks - (double)frame->begin_ticks) /
        frame_ticks;

    begin_normalized = (begin_normalized - view_begin) / view_span;
    end_normalized = (end_normalized - view_begin) / view_span;
    if (end_normalized <= 0.0 || begin_normalized >= 1.0)
    {
      continue;
    }

    if (begin_normalized < 0.0)
      begin_normalized = 0.0;
    if (end_normalized > 1.0)
      end_normalized = 1.0;

    y = timeline_rect.y + 2.0f + (float)sample->depth * row_height;
    if (y + row_height > timeline_rect.y + timeline_rect.h)
    {
      continue;
    }

    rect.x = timeline_rect.x + (float)begin_normalized * timeline_rect.w;
    rect.y = y;
    rect.w =
        (float)(end_normalized - begin_normalized) * timeline_rect.w;
    if (rect.w < 1.0f)
      rect.w = 1.0f;
    rect.h = row_height - 2.0f;

    if (ldk_ui_widget_color_view(
            ui, 0x50525400u + i, s_editor_profiler_kind_color(sample->kind),
            rect))
    {
      for (u32 node_i = 0; node_i < s_editor_profiler.tree_count; ++node_i)
      {
        if (s_editor_profiler.tree[node_i].sample_index == i)
        {
          s_editor_profiler.selected_tree_node = (i32)node_i;
          break;
        }
      }
    }
  }
  ui->clip_rect = previous_clip;
}

static void s_editor_profiler_tree_draw(LDKUIContext *ui)
{
  char label[256];
  LDKUIIcon icon = {0};

  if (!s_editor_profiler.tree_count)
  {
    ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
    ldk_ui_label(ui, "No CPU zones in selected frame.");
    return;
  }

  for (u32 i = 0; i < s_editor_profiler.tree_count; ++i)
  {
    LDKEditorProfilerTreeNode *node = &s_editor_profiler.tree[i];
    const LDKProfilerSampleRecord *sample =
        &s_editor_profiler.samples[node->sample_index];
    const LDKProfilerSourceRecord *source =
        s_editor_profiler_source_get(sample->source_id);
    u32 flags = 0;
    bool has_children = node->child_count != 0;

    if (!s_editor_profiler_node_visible(i))
    {
      continue;
    }

    if (!has_children)
    {
      flags |= LDK_UI_TREE_NODE_LEAF;
    }
    if ((i32)i == s_editor_profiler.selected_tree_node)
    {
      flags |= LDK_UI_TREE_NODE_SELECTED;
    }

    snprintf(label, sizeof(label), "%s%s    %.3f ms    self %.3f ms",
        source ? source->name : "<unknown>",
        (sample->flags & LDK_PROFILER_SAMPLE_FLAG_INCOMPLETE)
            ? " [incomplete]"
            : "",
        node->total_ms, node->self_ms);

    ldk_ui_push_id_u32(ui, i);
    u32 result = ldk_ui_tree_node_ex(ui, label, icon, node->expanded,
        sample->depth, flags);
    ldk_ui_pop_id(ui);

    if (result & LDK_UI_TREE_NODE_RESULT_CLICKED)
    {
      s_editor_profiler.selected_tree_node = (i32)i;
    }
    if ((result & LDK_UI_TREE_NODE_RESULT_TOGGLED) && has_children)
    {
      node->expanded = !node->expanded;
    }
  }
}

static void s_editor_profiler_selection_draw(
    LDKEditorContext *editor, LDKUIContext *ui)
{
  char text[512];

  if (s_editor_profiler.selected_tree_node < 0 ||
      (u32)s_editor_profiler.selected_tree_node >=
          s_editor_profiler.tree_count)
  {
    return;
  }

  const LDKEditorProfilerTreeNode *node =
      &s_editor_profiler.tree[s_editor_profiler.selected_tree_node];
  const LDKProfilerSampleRecord *sample =
      &s_editor_profiler.samples[node->sample_index];
  const LDKProfilerSourceRecord *source =
      s_editor_profiler_source_get(sample->source_id);

  ldk_ui_horizontal_line(ui);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, "Selection");

  snprintf(text, sizeof(text), "Type: %s", s_editor_profiler_kind_name(
                                              sample->kind));
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, text);

  snprintf(text, sizeof(text), "Total: %.3f ms    Self: %.3f ms",
      node->total_ms, node->self_ms);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, text);

  if (sample->thread_id > 0 &&
      sample->thread_id <= LDK_PROFILER_THREAD_CAPACITY &&
      s_editor_profiler.thread_valid[sample->thread_id - 1u])
  {
    snprintf(text, sizeof(text), "Thread: %s",
        s_editor_profiler.threads[sample->thread_id - 1u].name);
    ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
    ldk_ui_label(ui, text);
  }

  if (source)
  {
    if (source->function[0])
    {
      snprintf(text, sizeof(text), "Function: %s", source->function);
      ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
      ldk_ui_label(ui, text);
    }

    if (source->file[0])
    {
      snprintf(text, sizeof(text), "Source: %s:%u", source->file, source->line);
      ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
      ldk_ui_selectable_text(ui, text);

      if (ldk_ui_button(ui, "Copy Source Path"))
      {
        ldk_os_clipboard_text_set(editor->window, source->file);
      }
    }
  }
}

static void s_editor_profiler_counters_draw(LDKUIContext *ui)
{
  double values[LDK_PROFILER_COUNTER_CAPACITY] = {0};
  bool valid[LDK_PROFILER_COUNTER_CAPACITY] = {0};
  char text[256];
  u64 frame_index;

  if (s_editor_profiler.selected_frame >= s_editor_profiler.frame_count)
  {
    return;
  }

  frame_index =
      s_editor_profiler.frames[s_editor_profiler.selected_frame].index;

  for (u32 i = 0; i < s_editor_profiler.counter_sample_count; ++i)
  {
    const LDKProfilerCounterSampleRecord *sample =
        &s_editor_profiler.counter_samples[i];
    if (sample->frame_index != frame_index || sample->counter_id == 0 ||
        sample->counter_id > LDK_PROFILER_COUNTER_CAPACITY)
    {
      continue;
    }

    u32 index = sample->counter_id - 1u;
    if (sample->operation == LDK_PROFILER_COUNTER_ADD)
    {
      values[index] += sample->value;
    }
    else
    {
      values[index] = sample->value;
    }
    valid[index] = true;
  }

  bool any = false;
  for (u32 i = 0; i < LDK_PROFILER_COUNTER_CAPACITY; ++i)
  {
    if (valid[i] && s_editor_profiler.counter_valid[i])
    {
      if (!any)
      {
        ldk_ui_horizontal_line(ui);
        ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
        ldk_ui_label(ui, "Counters");
        any = true;
      }

      snprintf(text, sizeof(text), "%s: %.3f",
          s_editor_profiler.counters[i].name, values[i]);
      ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
      ldk_ui_label(ui, text);
    }
  }
}

static void s_editor_profiler_save_as(LDKEditorContext *editor)
{
  char selected[X_FS_PATH_MAX_LENGTH] = {0};
  XFSPath destination = {0};
  bool added_extension = false;

  if (editor->profiler_recording || !s_editor_profiler.loaded_path.length ||
      !ldk_os_dialog_show_save_file(editor->window, "Save Profiler Capture",
          "LDK Profiler Capture (*.ldkp)\0*.ldkp\0All Files (*.*)\0*.*\0\0",
          selected, sizeof(selected)))
  {
    return;
  }

  if (!x_fs_path_extension_cstr(selected).length)
  {
    size_t length = strlen(selected);
    if (length + sizeof(".ldkp") > sizeof(selected))
    {
      s_editor_profiler_error("Capture destination path is too long.");
      return;
    }
    memcpy(selected + length, ".ldkp", sizeof(".ldkp"));
    added_extension = true;
  }
  if (!x_fs_path_set(&destination, selected))
  {
    s_editor_profiler_error("Invalid capture destination path.");
    return;
  }
  x_fs_path_normalize(&destination);
  if (x_fs_path_compare(&destination, &s_editor_profiler.loaded_path) == 0)
  {
    return;
  }

  /* The native dialog confirms replacement before an extension is added. */
  if (added_extension && x_fs_path_is_file(&destination) &&
      !ldk_os_dialog_show_yes_no(editor->window, "Replace Capture",
          "The .ldkp file already exists. Replace it?"))
  {
    return;
  }
  if (!x_fs_file_copy(s_editor_profiler.loaded_path.buf, destination.buf))
  {
    s_editor_profiler_error("Failed to save capture copy.");
    return;
  }
  s_editor_profiler.error[0] = 0;
  s_editor_profiler.captures_dirty = true;
  ldki_editor_log_info(editor, "Profiler capture copy saved.");
}

static void s_editor_profiler_capture_list(
    LDKEditorContext *editor, LDKUIContext *ui)
{
  char label[320];

  if (editor && editor->project.loaded &&
      strcmp(s_editor_profiler.catalog_root.buf,
          editor->project.game_dll_path.buf) != 0)
  {
    s_editor_profiler.captures_dirty = true;
  }

  if (s_editor_profiler.captures_dirty)
  {
    s_editor_profiler_captures_refresh(editor);
  }

  ldk_ui_set_next_height(ui,
      ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT + 2 * LDK_UI_DEFAULT_PADDING));
  ldk_ui_begin_horizontal(ui);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "Refresh"))
  {
    s_editor_profiler_captures_refresh(editor);
    s_editor_profiler.auto_load_pending = true;
  }

  ldk_ui_set_next_disabled(ui, editor->profiler_recording);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "Open Capture..."))
  {
    char path[X_FS_PATH_MAX_LENGTH] = {0};
    if (ldk_os_dialog_show_open_file(editor->window, "Open Profiler Capture",
            "LDK Profiler Capture (*.ldkp)\0*.ldkp\0All Files (*.*)\0*.*\0\0",
            path, sizeof(path)))
    {
      s_editor_profiler_capture_load(path);
      if (s_editor_profiler.frame_count)
      {
        s_editor_profiler_frame_select(
            s_editor_profiler.frame_count - 1u);
      }
    }
  }
  ldk_ui_set_next_disabled(
      ui, editor->profiler_recording || !s_editor_profiler.loaded_path.length);
  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_button_flat(ui, "Save As..."))
  {
    s_editor_profiler_save_as(editor);
  }
  ldk_ui_spacer(ui);
  ldk_ui_end_horizontal(ui);

  if (!editor->project.loaded)
  {
    ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
    ldk_ui_label(ui, "Load a project to list captures.");
    return;
  }

  for (u32 i = 0; i < s_editor_profiler.capture_count; ++i)
  {
    XSlice basename =
        x_fs_path_basename_cstr(s_editor_profiler.captures[i].path.buf);
    size_t copy_length =
        basename.length < sizeof(label) - 1 ? basename.length
                                           : sizeof(label) - 1;
    memcpy(label, basename.ptr, copy_length);
    label[copy_length] = 0;

    ldk_ui_push_id_u32(ui, i);
    ldk_ui_set_next_disabled(ui, editor->profiler_recording);
    ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
    if (ldk_ui_button_flat(ui, label))
    {
      s_editor_profiler_capture_load(
          s_editor_profiler.captures[i].path.buf);
      if (s_editor_profiler.frame_count)
      {
        s_editor_profiler_frame_select(
            s_editor_profiler.frame_count - 1u);
      }
    }
    ldk_ui_pop_id(ui);
  }
}

void ldki_editor_profiler_update(void)
{
  bool open = ldki_editor_window_is_open(LDK_EDITOR_WINDOW_PROFILER);
  if (open && !s_editor_profiler.window_was_open)
  {
    s_editor_profiler.auto_load_pending = true;
  }
  s_editor_profiler.window_was_open = open;
  if (!open)
  {
    s_editor_profiler.splitter_dragging = false;
    s_editor_profiler.timeline_view_rect = (LDKUIRect){0};
  }
}

static void s_editor_profiler_auto_load(LDKEditorContext *editor)
{
  XFSPath path = {0};
  ldki_editor_profiler_path_get(editor, &path);
  if (strcmp(path.buf, s_editor_profiler.automatic_path.buf) != 0)
  {
    s_editor_profiler.automatic_path = path;
    s_editor_profiler.auto_load_pending = true;
    s_editor_profiler.captures_dirty = true;
    s_editor_profiler_capture_clear();
  }
  if (s_editor_profiler.observed_revision != editor->profiler_revision)
  {
    s_editor_profiler.observed_revision = editor->profiler_revision;
    s_editor_profiler.auto_load_pending = true;
    s_editor_profiler.captures_dirty = true;
  }
  if (editor->profiler_recording || !s_editor_profiler.auto_load_pending)
  {
    return;
  }
  s_editor_profiler.auto_load_pending = false;
  if (path.length && x_fs_path_is_file(&path))
  {
    if (s_editor_profiler_capture_load(path.buf) &&
        s_editor_profiler.frame_count)
    {
      s_editor_profiler_frame_select(s_editor_profiler.frame_count - 1u);
    }
  }
}

static float s_editor_profiler_overview_height(
    LDKUIContext *ui, float top, float available)
{
  if (s_editor_profiler.overview_ratio <= 0.0f)
  {
    s_editor_profiler.overview_ratio = 0.4f;
  }

  if (s_editor_profiler.splitter_dragging)
  {
    if (!ui->mouse || !ldk_os_mouse_button_is_pressed(
                          (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
    {
      s_editor_profiler.splitter_dragging = false;
    }
    else if (available > 0.0f)
    {
      LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
      s_editor_profiler.overview_ratio = ((float)cursor.y - top) / available;
      ui->cursor_type = LDK_CURSOR_SIZE_NS;
    }
  }

  /* Keep both panes reachable, including in a small docked window. */
  float minimum = available > 160.0f ? 80.0f / available : 0.25f;
  if (s_editor_profiler.overview_ratio < minimum)
  {
    s_editor_profiler.overview_ratio = minimum;
  }
  if (s_editor_profiler.overview_ratio > 1.0f - minimum)
  {
    s_editor_profiler.overview_ratio = 1.0f - minimum;
  }
  return available * s_editor_profiler.overview_ratio;
}

static void s_editor_profiler_splitter(LDKUIContext *ui)
{
  ldk_ui_set_next_height(ui, ldk_ui_px(6.0f));
  ldk_ui_horizontal_line(ui);
  LDKUIRect rect = ldk_ui_last_rect(ui);
  if (!ui->mouse || !ui->current_window)
  {
    return;
  }

  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
  bool hovered =
      ui->hovered_window_id == ui->current_window->id &&
      ldk_rectf_contains(&rect, (float)cursor.x, (float)cursor.y) &&
      ldk_rectf_contains(&ui->clip_rect, (float)cursor.x, (float)cursor.y);
  if (hovered || s_editor_profiler.splitter_dragging)
  {
    ui->cursor_type = LDK_CURSOR_SIZE_NS;
  }
  if (hovered && ldk_os_mouse_button_down(
                     (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT))
  {
    s_editor_profiler.splitter_dragging = true;
  }
}

static void s_editor_profiler_window(LDKEditor *opaque_editor, void *data)
{
  LDKEditorContext *editor = (LDKEditorContext *)opaque_editor;
  LDKUIContext *ui;
  char text[512];

  (void)data;
  if (!editor)
  {
    return;
  }

  ui = &editor->ui;
  s_editor_profiler_auto_load(editor);
  s_editor_profiler_capture_list(editor, ui);

  if (editor->profiler_recording)
  {
    ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
    ldk_ui_label(ui, "Recording Play capture. Stop Play to view results.");
    return;
  }

  if (s_editor_profiler.error[0])
  {
    ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
    ldk_ui_label(ui, s_editor_profiler.error);
  }

  if (!s_editor_profiler.loaded_path.length)
  {
    return;
  }

  ldk_ui_set_next_height(ui, ldk_ui_fill());
  ldk_ui_begin_vertical(ui);
  LDKUILayout *layout = ui->current_layout;
  float available = layout->content_rect.h - 6.0f - 2.0f * layout->spacing;
  if (available < 0.0f)
  {
    available = 0.0f;
  }
  float overview_height =
      s_editor_profiler_overview_height(ui, layout->content_rect.y, available);
  bool timeline_wheel = s_editor_profiler_timeline_wheel(ui);
  const LDKMouseState *saved_mouse = ui->mouse;
  LDKMouseState overview_mouse = {0};
  if (timeline_wheel)
  {
    overview_mouse = *saved_mouse;
    overview_mouse.wheel_delta = 0;
    ui->mouse = &overview_mouse;
  }
  ldk_ui_set_next_height(ui, ldk_ui_px(overview_height));
  s_editor_profiler.overview_scroll =
      ldk_ui_begin_scrollview(ui, s_editor_profiler.overview_scroll,
          LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  ui->mouse = saved_mouse;
  snprintf(text, sizeof(text), "Capture: %s",
      s_editor_profiler.loaded_path.buf);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_selectable_text(ui, text);

  snprintf(text, sizeof(text), "Frames: %u    CPU samples: %u    Dropped: %u",
      s_editor_profiler.frame_count, s_editor_profiler.sample_count,
      s_editor_profiler.dropped_records);
  ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
  ldk_ui_label(ui, text);

  s_editor_profiler_frame_graph(ui);

  if (s_editor_profiler.selected_frame < s_editor_profiler.frame_count)
  {
    const LDKEditorProfilerFrame *frame =
        &s_editor_profiler.frames[s_editor_profiler.selected_frame];
    snprintf(text, sizeof(text), "Frame %llu    %.3f ms",
        (unsigned long long)frame->index,
        s_editor_profiler_ticks_ms(frame->begin_ticks, frame->end_ticks));
    ldk_ui_set_next_height(ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
    ldk_ui_label(ui, text);
  }

  s_editor_profiler_timeline(ui);
  ldk_ui_end_scrollview(ui);

  s_editor_profiler_splitter(ui);
  ldk_ui_set_next_height(ui, ldk_ui_px(available - overview_height));
  s_editor_profiler.scroll = ldk_ui_begin_scrollview(ui,
      s_editor_profiler.scroll,
      LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);
  s_editor_profiler_tree_draw(ui);
  s_editor_profiler_selection_draw(editor, ui);
  s_editor_profiler_counters_draw(ui);
  ldk_ui_end_scrollview(ui);
  ldk_ui_end_vertical(ui);
}

static bool s_editor_profiler_register(LDKEditorContext *editor)
{
  LDKEditorWindow window = {
      .id = LDK_EDITOR_WINDOW_PROFILER,
      .title = "Profiler",
      .function = s_editor_profiler_window,
      .data = NULL};

  s_editor_profiler.captures_dirty = true;
  s_editor_profiler.auto_load_pending = true;
  if (!ldk_editor_window_add((LDKEditor *)editor, &window))
  {
    return false;
  }

  ldki_editor_window_hide(LDK_EDITOR_WINDOW_PROFILER);
  return true;
}
