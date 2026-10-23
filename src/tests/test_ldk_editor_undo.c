#include "../editor/ldk_editor_undo.h"

#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <string.h>

typedef struct TestUndoState
{
  float values[2];
  bool valid[2];
} TestUndoState;

static void *s_test_resolve(
    void *user, LDKEditorUndoKey key, LDKFieldType type, size_t size)
{
  TestUndoState *state = (TestUndoState *)user;
  (void)type;
  if (key.owner != 5 || key.property >= 2 ||
      !state->valid[key.property] || size != sizeof(float))
  {
    return NULL;
  }
  return &state->values[key.property];
}

static int test_undo_lifecycle(void)
{
  TestUndoState state = {{1.0f, 8.0f}, {true, true}};
  LDKEditorUndo undo;
  LDKEditorUndoKey a = {5, 0};
  LDKEditorUndoKey b = {5, 1};
  ldk_editor_undo_initialize(&undo, s_test_resolve, &state);

  ASSERT_FALSE(ldk_editor_undo_undo(&undo));
  ASSERT_FALSE(ldk_editor_undo_redo(&undo));
  ASSERT_TRUE(ldk_editor_undo_begin_change(
      &undo, a, LDK_FIELD_FLOAT, &state.values[0], sizeof(float)));
  ASSERT_FALSE(ldk_editor_undo_begin_change(
      &undo, b, LDK_FIELD_FLOAT, &state.values[1], sizeof(float)));
  ASSERT_FALSE(ldk_editor_undo_undo(&undo));
  ASSERT_FALSE(ldk_editor_undo_redo(&undo));
  state.values[0] = 2.0f;
  state.values[0] = 3.0f;
  ASSERT_TRUE(ldk_editor_undo_end_change(&undo, &state.values[0]));
  ASSERT_EQ(undo.count, 1);
  ASSERT_TRUE(ldk_editor_undo_undo(&undo));
  ASSERT_EQ(state.values[0], 1.0f);
  ASSERT_TRUE(ldk_editor_undo_redo(&undo));
  ASSERT_EQ(state.values[0], 3.0f);

  ASSERT_TRUE(ldk_editor_undo_begin_change(
      &undo, b, LDK_FIELD_FLOAT, &state.values[1], sizeof(float)));
  ASSERT_TRUE(ldk_editor_undo_end_change(&undo, &state.values[1]));
  ASSERT_EQ(undo.count, 1); /* No-op session. */

  ASSERT_TRUE(ldk_editor_undo_undo(&undo));
  ASSERT_TRUE(ldk_editor_undo_begin_change(
      &undo, b, LDK_FIELD_FLOAT, &state.values[1], sizeof(float)));
  state.values[1] = 9.0f;
  ASSERT_TRUE(ldk_editor_undo_end_change(&undo, &state.values[1]));
  ASSERT_EQ(undo.count, 1); /* New change dropped redo branch. */
  ASSERT_FALSE(ldk_editor_undo_redo(&undo));
  ASSERT_TRUE(ldk_editor_undo_undo(&undo));
  ASSERT_EQ(state.values[1], 8.0f);

  state.valid[1] = false;
  ASSERT_FALSE(ldk_editor_undo_redo(&undo));
  ASSERT_EQ(undo.position, 0);
  state.valid[1] = true;
  ASSERT_TRUE(ldk_editor_undo_redo(&undo));
  ASSERT_EQ(state.values[1], 9.0f);

  ldk_editor_undo_clear(&undo);
  ASSERT_EQ(undo.count, 0);
  ASSERT_FALSE(ldk_editor_undo_undo(&undo));
  ldk_editor_undo_terminate(&undo);
  return 0;
}

static int test_undo_relocated_property(void)
{
  TestUndoState original = {{2.0f, 0.0f}, {true, true}};
  TestUndoState relocated = {{5.0f, 0.0f}, {true, true}};
  LDKEditorUndo undo;
  LDKEditorUndoKey a = {5, 0};
  ldk_editor_undo_initialize(&undo, s_test_resolve, &original);
  ASSERT_TRUE(ldk_editor_undo_begin_change(
      &undo, a, LDK_FIELD_FLOAT, &original.values[0], sizeof(float)));
  original.values[0] = 5.0f;
  ASSERT_TRUE(ldk_editor_undo_end_change(&undo, &original.values[0]));
  undo.user = &relocated;
  ASSERT_TRUE(ldk_editor_undo_undo(&undo));
  ASSERT_EQ(relocated.values[0], 2.0f);
  ASSERT_TRUE(ldk_editor_undo_redo(&undo));
  ASSERT_EQ(relocated.values[0], 5.0f);
  ldk_editor_undo_terminate(&undo);
  return 0;
}

typedef struct TestGenericState
{
  float position[3];
  uint64_t asset;
} TestGenericState;

static void *s_test_generic_resolve(
    void *user, LDKEditorUndoKey key, LDKFieldType type, size_t size)
{
  TestGenericState *state = (TestGenericState *)user;
  if (key.owner != 9)
  {
    return NULL;
  }
  if (key.property == 1 && type == LDK_FIELD_VEC3 &&
      size == sizeof(state->position))
  {
    return state->position;
  }
  if (key.property == 2 && type == LDK_FIELD_ASSET_MESH &&
      size == sizeof(state->asset))
  {
    return &state->asset;
  }
  return NULL;
}

static int test_undo_composite_and_asset(void)
{
  TestGenericState state = {{1.0f, 2.0f, 3.0f}, 100};
  LDKEditorUndo undo;
  LDKEditorUndoKey vector_key = {9, 1};
  LDKEditorUndoKey asset_key = {9, 2};
  ldk_editor_undo_initialize(&undo, s_test_generic_resolve, &state);
  ASSERT_TRUE(ldk_editor_undo_begin_change(&undo, vector_key,
      LDK_FIELD_VEC3, state.position, sizeof(state.position)));
  state.position[0] = 4.0f;
  state.position[2] = 6.0f;
  ASSERT_TRUE(ldk_editor_undo_end_change(&undo, state.position));
  ASSERT_TRUE(ldk_editor_undo_begin_change(&undo, asset_key,
      LDK_FIELD_ASSET_MESH, &state.asset, sizeof(state.asset)));
  state.asset = 200;
  ASSERT_TRUE(ldk_editor_undo_end_change(&undo, &state.asset));
  ASSERT_EQ(undo.count, 2);
  ASSERT_TRUE(ldk_editor_undo_undo(&undo));
  ASSERT_EQ(state.asset, 100);
  ASSERT_TRUE(ldk_editor_undo_undo(&undo));
  ASSERT_EQ(state.position[0], 1.0f);
  ASSERT_EQ(state.position[2], 3.0f);
  ASSERT_TRUE(ldk_editor_undo_redo(&undo));
  ASSERT_EQ(state.position[0], 4.0f);
  ASSERT_TRUE(ldk_editor_undo_redo(&undo));
  ASSERT_EQ(state.asset, 200);
  ldk_editor_undo_terminate(&undo);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
    X_TEST(test_undo_lifecycle),
    X_TEST(test_undo_relocated_property),
    X_TEST(test_undo_composite_and_asset),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
