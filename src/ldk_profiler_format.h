#ifndef LDK_PROFILER_FORMAT_H
#define LDK_PROFILER_FORMAT_H

#include <ldk_common.h>

#define LDK_PROFILER_CAPTURE_MAGIC 0x504B444Cu
#define LDK_PROFILER_CAPTURE_VERSION 1u

#define LDK_PROFILER_SOURCE_NAME_CAPACITY 96
#define LDK_PROFILER_SOURCE_FUNCTION_CAPACITY 128
#define LDK_PROFILER_SOURCE_FILE_CAPACITY 260
#define LDK_PROFILER_THREAD_NAME_CAPACITY 64
#define LDK_PROFILER_COUNTER_NAME_CAPACITY 96

#define LDK_PROFILER_SOURCE_CAPACITY 2048u
#define LDK_PROFILER_COUNTER_CAPACITY 512u
#define LDK_PROFILER_THREAD_CAPACITY 64u
#define LDK_PROFILER_STACK_CAPACITY 128u

typedef enum LDKProfilerRecordType
{
  LDK_PROFILER_RECORD_SOURCE = 1,
  LDK_PROFILER_RECORD_THREAD = 2,
  LDK_PROFILER_RECORD_FRAME_BEGIN = 3,
  LDK_PROFILER_RECORD_FRAME_END = 4,
  LDK_PROFILER_RECORD_SAMPLE = 5,
  LDK_PROFILER_RECORD_COUNTER = 6,
  LDK_PROFILER_RECORD_COUNTER_SAMPLE = 7,
  LDK_PROFILER_RECORD_DROPPED = 8
} LDKProfilerRecordType;

typedef enum LDKProfilerSampleFlags
{
  LDK_PROFILER_SAMPLE_FLAG_NONE = 0,
  LDK_PROFILER_SAMPLE_FLAG_INCOMPLETE = 1 << 0
} LDKProfilerSampleFlags;

typedef enum LDKProfilerCounterOperation
{
  LDK_PROFILER_COUNTER_SET = 0,
  LDK_PROFILER_COUNTER_ADD = 1
} LDKProfilerCounterOperation;

#pragma pack(push, 1)

typedef struct LDKProfilerCaptureHeader
{
  u32 magic;
  u32 version;
  u64 tick_frequency;
  u64 capture_start_ticks;
  char build_type[32];
} LDKProfilerCaptureHeader;

typedef struct LDKProfilerRecordHeader
{
  u16 type;
  u16 size;
} LDKProfilerRecordHeader;

typedef struct LDKProfilerSourceRecord
{
  u32 id;
  u32 line;
  u8 kind;
  u8 reserved[3];
  char name[LDK_PROFILER_SOURCE_NAME_CAPACITY];
  char function[LDK_PROFILER_SOURCE_FUNCTION_CAPACITY];
  char file[LDK_PROFILER_SOURCE_FILE_CAPACITY];
} LDKProfilerSourceRecord;

typedef struct LDKProfilerThreadRecord
{
  u32 id;
  char name[LDK_PROFILER_THREAD_NAME_CAPACITY];
} LDKProfilerThreadRecord;

typedef struct LDKProfilerFrameRecord
{
  u64 frame_index;
  u64 ticks;
} LDKProfilerFrameRecord;

typedef struct LDKProfilerSampleRecord
{
  u32 source_id;
  u32 thread_id;
  u64 frame_index;
  u64 begin_ticks;
  u64 end_ticks;
  u16 depth;
  u8 kind;
  u8 flags;
} LDKProfilerSampleRecord;

typedef struct LDKProfilerCounterRecord
{
  u32 id;
  char name[LDK_PROFILER_COUNTER_NAME_CAPACITY];
} LDKProfilerCounterRecord;

typedef struct LDKProfilerCounterSampleRecord
{
  u32 counter_id;
  u32 thread_id;
  u64 frame_index;
  u64 ticks;
  double value;
  u8 operation;
  u8 reserved[7];
} LDKProfilerCounterSampleRecord;

typedef struct LDKProfilerDroppedRecord
{
  u32 thread_id;
  u32 count;
  u64 frame_index;
} LDKProfilerDroppedRecord;

#pragma pack(pop)

#endif // LDK_PROFILER_FORMAT_H
