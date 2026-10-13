/**
 * # System editor metadata
 *
 * Inspect engine-registered system metadata used by the editor.
 */

#ifndef LDK_SYSTEM_METADATA_H
#define LDK_SYSTEM_METADATA_H

#include <ldk_common.h>
#include <editor/ldk_component_metadata.h>

typedef struct LDKSystemMeta
{
  const char *name;
  u64 id;
  u32 flags;
  /* Stateful metadata uses the same field descriptions as components.
   * Stateless systems leave size, fields and field_count zero. */
  u32 size;
  const LDKComponentFieldMeta *fields;
  u32 field_count;
  const LDKComponentGroupMeta *groups;
  u32 group_count;
} LDKSystemMeta;

/**
 * @brief Count the engine system metadata.
 * @return Number of matching entries.
 */
LDK_API u32 ldk_engine_system_metadata_count(void);
/**
 * @brief Get the engine system metadata.
 * @param index Zero-based index of the selected item.
 * @return Pointer to the requested object or data; NULL when unavailable.
 */
LDK_API const LDKSystemMeta *ldk_engine_system_metadata_get(u32 index);

#endif

