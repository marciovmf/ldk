#if defined(LDK_SHAREDLIB)
#define X_IMPL_HASHTABLE
#endif // LDK_SHAREDLIB

#include <stdint.h>
#include <stdx/stdx_hashtable.h>

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

static uint64_t s_key_for_initial_slot(size_t slot, size_t capacity)
{
  uint64_t key = 0;

  for (;; ++key)
  {
    if (x_hashtable_hash_bytes(&key, sizeof(key)) % capacity == slot)
    {
      return key;
    }
  }
}

static int test_hashtable_reuses_deleted_slot_when_no_free_slots_remain(void)
{
  XHashtable *table = x_hashtable_create_ex(
      sizeof(uint64_t), false, false, sizeof(uint32_t), false, false);
  size_t capacity;
  uint64_t key;
  uint32_t value;

  ASSERT_TRUE(table != NULL);
  capacity = table->capacity;
  ASSERT_TRUE(capacity > 0u);

  /*
   * Turn every slot into a tombstone while keeping count == 0. Choosing a key
   * whose initial hash lands on each still-FREE slot forces that exact slot to
   * be occupied before it is removed.
   */
  for (size_t slot = 0; slot < capacity; ++slot)
  {
    key = s_key_for_initial_slot(slot, capacity);
    value = (uint32_t)slot;
    ASSERT_TRUE(x_hashtable_set(table, &key, &value));
    ASSERT_TRUE(x_hashtable_remove(table, &key));
  }

  ASSERT_TRUE(x_hashtable_count(table) == 0u);
  for (size_t slot = 0; slot < capacity; ++slot)
  {
    ASSERT_TRUE(table->entries[slot].state == X_HASH_ENTRY_DELETED);
  }

  key = UINT64_C(0x123456789abcdef0);
  value = 123u;
  ASSERT_TRUE(x_hashtable_set(table, &key, &value));
  ASSERT_TRUE(x_hashtable_count(table) == 1u);

  value = 0u;
  ASSERT_TRUE(x_hashtable_get(table, &key, &value));
  ASSERT_TRUE(value == 123u);

  x_hashtable_destroy(table);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] =
  {
    X_TEST(test_hashtable_reuses_deleted_slot_when_no_free_slots_remain),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
