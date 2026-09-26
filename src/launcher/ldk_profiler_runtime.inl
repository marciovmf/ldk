/* Shared engine collector; included only by src/ldk_profiler.c. */

#include "../ldk_profiler_format.h"

#include <stdx/stdx_filesystem.h>

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#ifndef LDK_PROFILER_CHUNK_SIZE
#define LDK_PROFILER_CHUNK_SIZE (64u * 1024u)
#endif

#ifndef LDK_PROFILER_CHUNK_COUNT
#define LDK_PROFILER_CHUNK_COUNT 32u
#endif

#ifndef LDK_PROFILER_STACK_CAPACITY
#define LDK_PROFILER_STACK_CAPACITY 128u
#endif

#ifndef LDK_PROFILER_SOURCE_CAPACITY
#define LDK_PROFILER_SOURCE_CAPACITY 2048u
#endif

#ifndef LDK_PROFILER_COUNTER_CAPACITY
#define LDK_PROFILER_COUNTER_CAPACITY 512u
#endif

#ifndef LDK_PROFILER_THREAD_CAPACITY
#define LDK_PROFILER_THREAD_CAPACITY 64u
#endif

typedef struct LDKProfilerChunk
{
  struct LDKProfilerChunk *next;
  u32 size;
  u8 data[LDK_PROFILER_CHUNK_SIZE];
} LDKProfilerChunk;

typedef struct LDKProfilerSourceInfo
{
  LDKProfilerSourceRecord record;
} LDKProfilerSourceInfo;

typedef struct LDKProfilerCounterInfo
{
  LDKProfilerCounterRecord record;
} LDKProfilerCounterInfo;

typedef struct LDKProfilerOpenZone
{
  u32 source_id;
  u64 begin_ticks;
  u16 depth;
  u8 kind;
} LDKProfilerOpenZone;

typedef struct LDKProfilerThreadState
{
  LDKProfilerChunk *chunk;
  LDKProfilerOpenZone stack[LDK_PROFILER_STACK_CAPACITY];
  u32 depth;
  u32 id;
  u32 dropped;
  char name[LDK_PROFILER_THREAD_NAME_CAPACITY];
} LDKProfilerThreadState;

typedef struct LDKProfilerRuntime
{
  CRITICAL_SECTION queue_lock;
  CRITICAL_SECTION registry_lock;
  HANDLE writer_event;
  HANDLE writer_idle_event;
  HANDLE writer_thread;

  LDKProfilerChunk *chunks;
  LDKProfilerChunk *free_chunks;
  LDKProfilerChunk *queue_head;
  LDKProfilerChunk *queue_tail;

  LDKProfilerSourceInfo sources[LDK_PROFILER_SOURCE_CAPACITY];
  LDKProfilerCounterInfo counters[LDK_PROFILER_COUNTER_CAPACITY];
  LDKProfilerThreadState *threads[LDK_PROFILER_THREAD_CAPACITY];

  FILE *file;
  XFSPath profiling_directory;
  XFSPath capture_path;

  volatile LONG active;
  volatile LONG toggle_requested;
  volatile LONG shutdown;
  volatile LONG queued_chunks;

  u32 source_count;
  u32 counter_count;
  u32 thread_count;
  u64 frame_index;
  u64 tick_frequency;
  bool initialized;
} LDKProfilerRuntime;

static LDKProfilerRuntime s_profiler = {0};
static __declspec(thread) LDKProfilerThreadState *s_profiler_thread = NULL;
static LDKProfilerSource s_profiler_frame_source = {0};

static void s_profiler_emit_thread(
    LDKProfilerThreadState *writer, const LDKProfilerThreadState *thread);

static void s_profiler_cstr_copy(char *destination, size_t capacity,
    const char *source)
{
  if (!destination || capacity == 0)
  {
    return;
  }

  destination[0] = 0;
  if (!source)
  {
    return;
  }

  strncpy(destination, source, capacity - 1);
  destination[capacity - 1] = 0;
}

static LDKProfilerChunk *s_profiler_chunk_acquire(void)
{
  LDKProfilerChunk *chunk = NULL;

  EnterCriticalSection(&s_profiler.queue_lock);
  if (s_profiler.free_chunks)
  {
    chunk = s_profiler.free_chunks;
    s_profiler.free_chunks = chunk->next;
    chunk->next = NULL;
    chunk->size = 0;
  }
  LeaveCriticalSection(&s_profiler.queue_lock);
  return chunk;
}

static void s_profiler_chunk_release(LDKProfilerChunk *chunk)
{
  if (!chunk)
  {
    return;
  }

  EnterCriticalSection(&s_profiler.queue_lock);
  chunk->next = s_profiler.free_chunks;
  s_profiler.free_chunks = chunk;
  LeaveCriticalSection(&s_profiler.queue_lock);
}

static void s_profiler_chunk_publish(LDKProfilerThreadState *thread)
{
  LDKProfilerChunk *chunk;

  if (!thread || !thread->chunk || thread->chunk->size == 0)
  {
    return;
  }

  chunk = thread->chunk;
  thread->chunk = NULL;

  ResetEvent(s_profiler.writer_idle_event);
  InterlockedIncrement(&s_profiler.queued_chunks);

  EnterCriticalSection(&s_profiler.queue_lock);
  chunk->next = NULL;
  if (s_profiler.queue_tail)
  {
    s_profiler.queue_tail->next = chunk;
  }
  else
  {
    s_profiler.queue_head = chunk;
  }
  s_profiler.queue_tail = chunk;
  LeaveCriticalSection(&s_profiler.queue_lock);

  SetEvent(s_profiler.writer_event);
}

static LDKProfilerChunk *s_profiler_queue_pop(void)
{
  LDKProfilerChunk *chunk = NULL;

  EnterCriticalSection(&s_profiler.queue_lock);
  if (s_profiler.queue_head)
  {
    chunk = s_profiler.queue_head;
    s_profiler.queue_head = chunk->next;
    if (!s_profiler.queue_head)
    {
      s_profiler.queue_tail = NULL;
    }
    chunk->next = NULL;
  }
  LeaveCriticalSection(&s_profiler.queue_lock);
  return chunk;
}

static DWORD WINAPI s_profiler_writer_thread(void *user)
{
  (void)user;

  for (;;)
  {
    WaitForSingleObject(s_profiler.writer_event, INFINITE);

    for (;;)
    {
      LDKProfilerChunk *chunk = s_profiler_queue_pop();
      if (!chunk)
      {
        break;
      }

      if (s_profiler.file && chunk->size)
      {
        fwrite(chunk->data, 1, chunk->size, s_profiler.file);
      }

      s_profiler_chunk_release(chunk);

      if (InterlockedDecrement(&s_profiler.queued_chunks) == 0)
      {
        SetEvent(s_profiler.writer_idle_event);
      }
    }

    if (InterlockedCompareExchange(&s_profiler.shutdown, 0, 0) != 0)
    {
      break;
    }
  }

  SetEvent(s_profiler.writer_idle_event);
  return 0;
}

static bool s_profiler_thread_register(LDKProfilerThreadState *thread)
{
  if (!thread)
  {
    return false;
  }

  EnterCriticalSection(&s_profiler.registry_lock);
  if (s_profiler.thread_count >= LDK_PROFILER_THREAD_CAPACITY)
  {
    LeaveCriticalSection(&s_profiler.registry_lock);
    return false;
  }

  thread->id = s_profiler.thread_count + 1u;
  if (!thread->name[0])
  {
    snprintf(thread->name, sizeof(thread->name), "Thread %u", thread->id);
  }
  s_profiler.threads[s_profiler.thread_count++] = thread;
  LeaveCriticalSection(&s_profiler.registry_lock);

  if (InterlockedCompareExchange(&s_profiler.active, 0, 0) != 0)
  {
    s_profiler_emit_thread(thread, thread);
  }
  return true;
}

static LDKProfilerThreadState *s_profiler_thread_get(void)
{
  if (s_profiler_thread)
  {
    return s_profiler_thread;
  }

  s_profiler_thread =
      (LDKProfilerThreadState *)calloc(1, sizeof(LDKProfilerThreadState));
  if (!s_profiler_thread)
  {
    return NULL;
  }

  if (!s_profiler_thread_register(s_profiler_thread))
  {
    free(s_profiler_thread);
    s_profiler_thread = NULL;
  }

  return s_profiler_thread;
}

static bool s_profiler_append_bytes(
    LDKProfilerThreadState *thread, const void *data, u32 size)
{
  if (!thread || !data || size == 0 || size > LDK_PROFILER_CHUNK_SIZE)
  {
    return false;
  }

  if (!thread->chunk)
  {
    thread->chunk = s_profiler_chunk_acquire();
    if (!thread->chunk)
    {
      thread->dropped += 1u;
      return false;
    }
  }

  if (thread->chunk->size + size > LDK_PROFILER_CHUNK_SIZE)
  {
    s_profiler_chunk_publish(thread);
    thread->chunk = s_profiler_chunk_acquire();
    if (!thread->chunk)
    {
      thread->dropped += 1u;
      return false;
    }
  }

  memcpy(thread->chunk->data + thread->chunk->size, data, size);
  thread->chunk->size += size;
  return true;
}

static bool s_profiler_append_record(LDKProfilerThreadState *thread,
    LDKProfilerRecordType type, const void *payload, u16 payload_size)
{
  LDKProfilerRecordHeader header = {
      .type = (u16)type, .size = payload_size};

  if (!s_profiler_append_bytes(thread, &header, (u32)sizeof(header)) ||
      (payload_size &&
          !s_profiler_append_bytes(thread, payload, (u32)payload_size)))
  {
    return false;
  }

  return true;
}

static void s_profiler_emit_thread(
    LDKProfilerThreadState *writer, const LDKProfilerThreadState *thread)
{
  LDKProfilerThreadRecord record = {0};
  if (!writer || !thread)
  {
    return;
  }

  record.id = thread->id;
  s_profiler_cstr_copy(record.name, sizeof(record.name), thread->name);
  s_profiler_append_record(
      writer, LDK_PROFILER_RECORD_THREAD, &record, (u16)sizeof(record));
}

static void s_profiler_emit_source(
    LDKProfilerThreadState *thread, const LDKProfilerSourceRecord *record)
{
  if (thread && record)
  {
    s_profiler_append_record(
        thread, LDK_PROFILER_RECORD_SOURCE, record, (u16)sizeof(*record));
  }
}

static void s_profiler_emit_counter(
    LDKProfilerThreadState *thread, const LDKProfilerCounterRecord *record)
{
  if (thread && record)
  {
    s_profiler_append_record(
        thread, LDK_PROFILER_RECORD_COUNTER, record, (u16)sizeof(*record));
  }
}

static u32 s_profiler_source_resolve(LDKProfilerSource *source,
    LDKProfilerZoneKind kind, const char *name, const char *file,
    const char *function, u32 line, LDKProfilerThreadState *thread)
{
  LDKProfilerSourceRecord record = {0};
  i32 id;

  if (!source)
  {
    return 0;
  }

  id = source->id;
  if (id > 0)
  {
    return (u32)id;
  }

  EnterCriticalSection(&s_profiler.registry_lock);
  id = source->id;
  if (id <= 0)
  {
    if (s_profiler.source_count >= LDK_PROFILER_SOURCE_CAPACITY)
    {
      LeaveCriticalSection(&s_profiler.registry_lock);
      return 0;
    }

    id = (i32)(s_profiler.source_count + 1u);
    record.id = (u32)id;
    record.line = line;
    record.kind = (u8)kind;
    s_profiler_cstr_copy(record.name, sizeof(record.name), name);
    s_profiler_cstr_copy(record.function, sizeof(record.function), function);
    s_profiler_cstr_copy(record.file, sizeof(record.file), file);
    s_profiler.sources[s_profiler.source_count++].record = record;
    source->id = id;
  }
  else
  {
    record = s_profiler.sources[(u32)id - 1u].record;
  }
  LeaveCriticalSection(&s_profiler.registry_lock);

  if (record.id != 0 && InterlockedCompareExchange(&s_profiler.active, 0, 0))
  {
    s_profiler_emit_source(thread, &record);
  }
  return (u32)id;
}

static u32 s_profiler_counter_resolve(LDKProfilerCounter *counter,
    const char *name, LDKProfilerThreadState *thread)
{
  LDKProfilerCounterRecord record = {0};
  i32 id;

  if (!counter)
  {
    return 0;
  }

  id = counter->id;
  if (id > 0)
  {
    return (u32)id;
  }

  EnterCriticalSection(&s_profiler.registry_lock);
  id = counter->id;
  if (id <= 0)
  {
    if (s_profiler.counter_count >= LDK_PROFILER_COUNTER_CAPACITY)
    {
      LeaveCriticalSection(&s_profiler.registry_lock);
      return 0;
    }

    id = (i32)(s_profiler.counter_count + 1u);
    record.id = (u32)id;
    s_profiler_cstr_copy(record.name, sizeof(record.name), name);
    s_profiler.counters[s_profiler.counter_count++].record = record;
    counter->id = id;
  }
  else
  {
    record = s_profiler.counters[(u32)id - 1u].record;
  }
  LeaveCriticalSection(&s_profiler.registry_lock);

  if (record.id != 0 && InterlockedCompareExchange(&s_profiler.active, 0, 0))
  {
    s_profiler_emit_counter(thread, &record);
  }
  return (u32)id;
}

static void s_profiler_dropped_emit(LDKProfilerThreadState *thread)
{
  LDKProfilerDroppedRecord record;
  u32 dropped;

  if (!thread || thread->dropped == 0)
  {
    return;
  }

  dropped = thread->dropped;
  thread->dropped = 0;
  record.thread_id = thread->id;
  record.count = dropped;
  record.frame_index = s_profiler.frame_index;
  s_profiler_append_record(
      thread, LDK_PROFILER_RECORD_DROPPED, &record, (u16)sizeof(record));
}

static void s_profiler_zone_close_top(
    LDKProfilerThreadState *thread, u64 end_ticks, u8 flags)
{
  LDKProfilerOpenZone zone;
  LDKProfilerSampleRecord record;

  if (!thread || thread->depth == 0)
  {
    return;
  }

  zone = thread->stack[--thread->depth];
  record.source_id = zone.source_id;
  record.thread_id = thread->id;
  record.frame_index = s_profiler.frame_index;
  record.begin_ticks = zone.begin_ticks;
  record.end_ticks = end_ticks;
  record.depth = zone.depth;
  record.kind = zone.kind;
  record.flags = flags;

  s_profiler_append_record(
      thread, LDK_PROFILER_RECORD_SAMPLE, &record, (u16)sizeof(record));
}

static bool s_profiler_capture_path_make(XFSPath *out)
{
  time_t now;
  struct tm local_time;
  char name[96];

  if (!out)
  {
    return false;
  }

  now = time(NULL);
  if (localtime_s(&local_time, &now) != 0)
  {
    return false;
  }

  for (u32 suffix = 0; suffix < 1000u; ++suffix)
  {
    if (suffix == 0)
    {
      snprintf(name, sizeof(name),
          "capture_%04d%02d%02d_%02d%02d%02d.ldkp",
          local_time.tm_year + 1900, local_time.tm_mon + 1,
          local_time.tm_mday, local_time.tm_hour, local_time.tm_min,
          local_time.tm_sec);
    }
    else
    {
      snprintf(name, sizeof(name),
          "capture_%04d%02d%02d_%02d%02d%02d_%03u.ldkp",
          local_time.tm_year + 1900, local_time.tm_mon + 1,
          local_time.tm_mday, local_time.tm_hour, local_time.tm_min,
          local_time.tm_sec, suffix);
    }

    if (!x_fs_path(out, s_profiler.profiling_directory.buf, name))
    {
      return false;
    }

    if (!x_fs_path_exists_cstr(out->buf))
    {
      return true;
    }
  }

  return false;
}

bool ldk_profiler_capture_start(const char *path)
{
  LDKProfilerCaptureHeader header = {0};
  LARGE_INTEGER frequency;
  LDKProfilerThreadState *thread;
  XFSPath directory = {0};

  if (!s_profiler.initialized || s_profiler.file)
  {
    return false;
  }
  thread = s_profiler_thread_get();
  if (path && path[0])
  {
    if (!x_fs_path_set(&s_profiler.capture_path, path) ||
        !x_fs_path_dirname(&s_profiler.capture_path, &directory) ||
        !x_fs_directory_create_recursive(directory.buf))
    {
      return false;
    }
  }
  else if (!s_profiler_capture_path_make(&s_profiler.capture_path))
  {
    return false;
  }

  if (!thread)
  {
    return false;
  }

  s_profiler.file = fopen(s_profiler.capture_path.buf, "wb");
  if (!s_profiler.file)
  {
    return false;
  }

  QueryPerformanceFrequency(&frequency);
  s_profiler.tick_frequency = (u64)frequency.QuadPart;
  s_profiler.frame_index = 0;

  header.magic = LDK_PROFILER_CAPTURE_MAGIC;
  header.version = LDK_PROFILER_CAPTURE_VERSION;
  header.tick_frequency = s_profiler.tick_frequency;
  header.capture_start_ticks = ldk_os_time_ticks_get();
  s_profiler_cstr_copy(header.build_type, sizeof(header.build_type),
      LDK_BUILD_TYPE);
  if (fwrite(&header, 1, sizeof(header), s_profiler.file) != sizeof(header))
  {
    fclose(s_profiler.file);
    s_profiler.file = NULL;
    return false;
  }

  InterlockedExchange(&s_profiler.active, 1);

  EnterCriticalSection(&s_profiler.registry_lock);
  for (u32 i = 0; i < s_profiler.source_count; ++i)
  {
    s_profiler_emit_source(thread, &s_profiler.sources[i].record);
  }
  for (u32 i = 0; i < s_profiler.counter_count; ++i)
  {
    s_profiler_emit_counter(thread, &s_profiler.counters[i].record);
  }
  for (u32 i = 0; i < s_profiler.thread_count; ++i)
  {
    s_profiler_emit_thread(thread, s_profiler.threads[i]);
  }
  LeaveCriticalSection(&s_profiler.registry_lock);

  ldk_log_info("Profiler capture started: %s\n", s_profiler.capture_path.buf);
  return true;
}

void ldk_profiler_capture_stop(void)
{
  if (!s_profiler.file)
  {
    InterlockedExchange(&s_profiler.active, 0);
    return;
  }

  InterlockedExchange(&s_profiler.active, 0);

  EnterCriticalSection(&s_profiler.registry_lock);
  for (u32 i = 0; i < s_profiler.thread_count; ++i)
  {
    LDKProfilerThreadState *thread = s_profiler.threads[i];
    if (!thread)
    {
      continue;
    }

    while (thread->depth)
    {
      s_profiler_zone_close_top(
          thread, ldk_os_time_ticks_get(), LDK_PROFILER_SAMPLE_FLAG_INCOMPLETE);
    }
    s_profiler_dropped_emit(thread);
    s_profiler_chunk_publish(thread);
  }
  LeaveCriticalSection(&s_profiler.registry_lock);

  if (InterlockedCompareExchange(&s_profiler.queued_chunks, 0, 0) != 0)
  {
    SetEvent(s_profiler.writer_event);
    WaitForSingleObject(s_profiler.writer_idle_event, INFINITE);
  }

  fflush(s_profiler.file);
  fclose(s_profiler.file);
  s_profiler.file = NULL;
  ldk_log_info("Profiler capture stopped: %s\n", s_profiler.capture_path.buf);
}

bool ldk_profiler_initialize(const char *runtree_path)
{
  LARGE_INTEGER frequency;

  if (s_profiler.initialized)
  {
    return true;
  }

  if (!runtree_path || !runtree_path[0] ||
      !x_fs_path(&s_profiler.profiling_directory, runtree_path, "profiling") ||
      !x_fs_directory_create_recursive(s_profiler.profiling_directory.buf))
  {
    return false;
  }

  InitializeCriticalSection(&s_profiler.queue_lock);
  InitializeCriticalSection(&s_profiler.registry_lock);
  s_profiler.initialized = true;

  s_profiler.writer_event = CreateEvent(NULL, FALSE, FALSE, NULL);
  s_profiler.writer_idle_event = CreateEvent(NULL, TRUE, TRUE, NULL);
  if (!s_profiler.writer_event || !s_profiler.writer_idle_event)
  {
    ldk_profiler_terminate();
    return false;
  }

  s_profiler.chunks = (LDKProfilerChunk *)_aligned_malloc(
      sizeof(LDKProfilerChunk) * LDK_PROFILER_CHUNK_COUNT, 64);
  if (!s_profiler.chunks)
  {
    ldk_profiler_terminate();
    return false;
  }
  memset(s_profiler.chunks, 0,
      sizeof(LDKProfilerChunk) * LDK_PROFILER_CHUNK_COUNT);

  for (u32 i = 0; i < LDK_PROFILER_CHUNK_COUNT; ++i)
  {
    s_profiler.chunks[i].next = s_profiler.free_chunks;
    s_profiler.free_chunks = &s_profiler.chunks[i];
  }

  QueryPerformanceFrequency(&frequency);
  s_profiler.tick_frequency = (u64)frequency.QuadPart;
  s_profiler.writer_thread =
      CreateThread(NULL, 0, s_profiler_writer_thread, NULL, 0, NULL);
  if (!s_profiler.writer_thread)
  {
    ldk_profiler_terminate();
    return false;
  }

  ldk_profiler_thread_name_set("Main");
  return true;
}

void ldk_profiler_terminate(void)
{
  if (s_profiler.file)
  {
    ldk_profiler_capture_stop();
  }

  if (s_profiler.writer_thread)
  {
    InterlockedExchange(&s_profiler.shutdown, 1);
    SetEvent(s_profiler.writer_event);
    WaitForSingleObject(s_profiler.writer_thread, INFINITE);
    CloseHandle(s_profiler.writer_thread);
  }

  if (s_profiler.writer_event)
  {
    CloseHandle(s_profiler.writer_event);
  }
  if (s_profiler.writer_idle_event)
  {
    CloseHandle(s_profiler.writer_idle_event);
  }

  for (u32 i = 0; i < s_profiler.thread_count; ++i)
  {
    free(s_profiler.threads[i]);
  }

  if (s_profiler.chunks)
  {
    _aligned_free(s_profiler.chunks);
  }

  if (s_profiler.initialized)
  {
    DeleteCriticalSection(&s_profiler.queue_lock);
    DeleteCriticalSection(&s_profiler.registry_lock);
  }

  memset(&s_profiler, 0, sizeof(s_profiler));
  s_profiler_thread = NULL;
}

void ldk_profiler_capture_collect(bool enabled)
{
  InterlockedExchange(&s_profiler.active, enabled && s_profiler.file ? 1 : 0);
}

void ldk_profiler_capture_toggle(void)
{
  if (s_profiler.initialized)
  {
    InterlockedExchange(&s_profiler.toggle_requested, 1);
  }
}

bool ldk_profiler_capture_is_active(void)
{
  return InterlockedCompareExchange(&s_profiler.active, 0, 0) != 0;
}

const char *ldk_profiler_capture_path_get(void)
{
  return s_profiler.capture_path.buf;
}

void ldk_profiler_thread_name_set(const char *name)
{
  LDKProfilerThreadState *thread = s_profiler_thread_get();
  if (!thread)
  {
    return;
  }

  s_profiler_cstr_copy(thread->name, sizeof(thread->name), name);
}

void ldk_profiler_frame_begin(void)
{
  LDKProfilerThreadState *thread;
  LDKProfilerFrameRecord record;

  if (!s_profiler.initialized)
  {
    return;
  }

  if (InterlockedExchange(&s_profiler.toggle_requested, 0) != 0)
  {
    if (ldk_profiler_capture_is_active())
    {
      ldk_profiler_capture_stop();
    }
    else
    {
      ldk_profiler_capture_start(NULL);
    }
  }

  if (!ldk_profiler_capture_is_active())
  {
    return;
  }

  thread = s_profiler_thread_get();
  if (!thread)
  {
    return;
  }

  s_profiler.frame_index += 1u;
  record.frame_index = s_profiler.frame_index;
  record.ticks = ldk_os_time_ticks_get();
  s_profiler_append_record(thread, LDK_PROFILER_RECORD_FRAME_BEGIN, &record,
      (u16)sizeof(record));

  ldk_profiler_zone_begin_source(&s_profiler_frame_source,
      LDK_PROFILER_ZONE_FRAME, "Frame", "", "ldk_engine_frame", 0);
}

void ldk_profiler_frame_end(void)
{
  LDKProfilerThreadState *thread;
  LDKProfilerFrameRecord record;

  if (!ldk_profiler_capture_is_active())
  {
    return;
  }

  thread = s_profiler_thread_get();
  if (!thread)
  {
    return;
  }

  ldk_profiler_zone_end_kind(LDK_PROFILER_ZONE_FRAME);

  record.frame_index = s_profiler.frame_index;
  record.ticks = ldk_os_time_ticks_get();
  s_profiler_append_record(thread, LDK_PROFILER_RECORD_FRAME_END, &record,
      (u16)sizeof(record));
  s_profiler_dropped_emit(thread);
  s_profiler_chunk_publish(thread);
}

void ldk_profiler_zone_begin_source(LDKProfilerSource *source,
    LDKProfilerZoneKind kind, const char *name, const char *file,
    const char *function, u32 line)
{
  LDKProfilerThreadState *thread;
  LDKProfilerOpenZone *zone;
  u32 source_id;

  if (!ldk_profiler_capture_is_active())
  {
    return;
  }

  thread = s_profiler_thread_get();
  if (!thread)
  {
    return;
  }

  source_id = s_profiler_source_resolve(
      source, kind, name, file, function, line, thread);
  if (!source_id)
  {
    thread->dropped += 1u;
    return;
  }

  if (thread->depth >= LDK_PROFILER_STACK_CAPACITY)
  {
    thread->dropped += 1u;
    return;
  }

  zone = &thread->stack[thread->depth];
  zone->source_id = source_id;
  zone->begin_ticks = ldk_os_time_ticks_get();
  zone->depth = (u16)thread->depth;
  zone->kind = (u8)kind;
  thread->depth += 1u;
}

void ldk_profiler_zone_end(void)
{
  LDKProfilerThreadState *thread;

  if (!ldk_profiler_capture_is_active())
  {
    return;
  }

  thread = s_profiler_thread_get();
  if (!thread || thread->depth == 0)
  {
    return;
  }

  if (thread->stack[thread->depth - 1u].kind !=
      (u8)LDK_PROFILER_ZONE_USER)
  {
    thread->dropped += 1u;
    return;
  }

  s_profiler_zone_close_top(
      thread, ldk_os_time_ticks_get(), LDK_PROFILER_SAMPLE_FLAG_NONE);
}

void ldk_profiler_zone_end_kind(LDKProfilerZoneKind kind)
{
  LDKProfilerThreadState *thread;
  u64 ticks;

  if (!ldk_profiler_capture_is_active())
  {
    return;
  }

  thread = s_profiler_thread_get();
  if (!thread || thread->depth == 0)
  {
    return;
  }

  ticks = ldk_os_time_ticks_get();

  while (thread->depth &&
         thread->stack[thread->depth - 1u].kind != (u8)kind)
  {
    s_profiler_zone_close_top(
        thread, ticks, LDK_PROFILER_SAMPLE_FLAG_INCOMPLETE);
  }

  if (thread->depth)
  {
    s_profiler_zone_close_top(
        thread, ticks, LDK_PROFILER_SAMPLE_FLAG_NONE);
  }
}

static void s_profiler_counter_sample(LDKProfilerCounter *counter,
    const char *name, double value, LDKProfilerCounterOperation operation)
{
  LDKProfilerThreadState *thread;
  LDKProfilerCounterSampleRecord record = {0};
  u32 counter_id;

  if (!ldk_profiler_capture_is_active())
  {
    return;
  }

  thread = s_profiler_thread_get();
  if (!thread)
  {
    return;
  }

  counter_id = s_profiler_counter_resolve(counter, name, thread);
  if (!counter_id)
  {
    thread->dropped += 1u;
    return;
  }

  record.counter_id = counter_id;
  record.thread_id = thread->id;
  record.frame_index = s_profiler.frame_index;
  record.ticks = ldk_os_time_ticks_get();
  record.value = value;
  record.operation = (u8)operation;
  s_profiler_append_record(thread, LDK_PROFILER_RECORD_COUNTER_SAMPLE, &record,
      (u16)sizeof(record));
}

void ldk_profiler_counter_set_source(
    LDKProfilerCounter *counter, const char *name, double value)
{
  s_profiler_counter_sample(counter, name, value, LDK_PROFILER_COUNTER_SET);
}

void ldk_profiler_counter_add_source(
    LDKProfilerCounter *counter, const char *name, double value)
{
  s_profiler_counter_sample(counter, name, value, LDK_PROFILER_COUNTER_ADD);
}
