#include "../editor/ldk_editor_undo.h"
#include <ldk_material.h>

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

  ASSERT_FALSE(ldk_editor_undo_can_undo(&undo));
  ASSERT_FALSE(ldk_editor_undo_can_redo(&undo));
  ASSERT_FALSE(ldk_editor_undo_has_active_change(&undo));
  ASSERT_FALSE(ldk_editor_undo_undo(&undo));
  ASSERT_FALSE(ldk_editor_undo_redo(&undo));
  ASSERT_TRUE(ldk_editor_undo_begin_change(
      &undo, a, LDK_FIELD_FLOAT, &state.values[0], sizeof(float)));
  ASSERT_FALSE(ldk_editor_undo_begin_change(
      &undo, b, LDK_FIELD_FLOAT, &state.values[1], sizeof(float)));
  ASSERT_TRUE(ldk_editor_undo_has_active_change(&undo));
  ASSERT_FALSE(ldk_editor_undo_can_undo(&undo));
  ASSERT_FALSE(ldk_editor_undo_can_redo(&undo));
  ASSERT_FALSE(ldk_editor_undo_undo(&undo));
  ASSERT_FALSE(ldk_editor_undo_redo(&undo));
  state.values[0] = 2.0f;
  state.values[0] = 3.0f;
  ASSERT_TRUE(ldk_editor_undo_end_change(&undo, &state.values[0]));
  ASSERT_EQ(undo.count, 1);
  ASSERT_FALSE(ldk_editor_undo_has_active_change(&undo));
  ASSERT_TRUE(ldk_editor_undo_can_undo(&undo));
  ASSERT_FALSE(ldk_editor_undo_can_redo(&undo));
  ASSERT_TRUE(ldk_editor_undo_undo(&undo));
  ASSERT_FALSE(ldk_editor_undo_can_undo(&undo));
  ASSERT_TRUE(ldk_editor_undo_can_redo(&undo));
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

static int test_undo_end_after_caller_write(void)
{
  TestUndoState state = {{10.0f, 0.0f}, {true, true}};
  LDKEditorUndo undo;
  LDKEditorUndoKey key = {5, 0};
  float returned_by_row;
  ldk_editor_undo_initialize(&undo, s_test_resolve, &state);
  ASSERT_TRUE(ldk_editor_undo_begin_change(
      &undo, key, LDK_FIELD_FLOAT, &state.values[0], sizeof(float)));
  returned_by_row = 42.0f;
  state.values[0] = returned_by_row; /* Inspector writes after row returns. */
  ASSERT_TRUE(ldk_editor_undo_end_change_resolved(&undo));
  ASSERT_EQ(undo.count, 1);
  ASSERT_TRUE(ldk_editor_undo_undo(&undo));
  ASSERT_EQ(state.values[0], 10.0f);
  ASSERT_TRUE(ldk_editor_undo_redo(&undo));
  ASSERT_EQ(state.values[0], 42.0f);

  ASSERT_TRUE(ldk_editor_undo_begin_change(
      &undo, key, LDK_FIELD_FLOAT, &state.values[0], sizeof(float)));
  ASSERT_TRUE(ldk_editor_undo_end_change_resolved(&undo));
  ASSERT_EQ(undo.count, 1); /* No-op. */

  ASSERT_TRUE(ldk_editor_undo_begin_change(
      &undo, key, LDK_FIELD_FLOAT, &state.values[0], sizeof(float)));
  state.valid[0] = false;
  ASSERT_FALSE(ldk_editor_undo_end_change_resolved(&undo));
  ASSERT_EQ(undo.count, 1); /* Cannot resolve: no history entry. */
  state.valid[0] = true;
  ASSERT_TRUE(ldk_editor_undo_end_change_resolved(&undo));
  ldk_editor_undo_terminate(&undo);
  return 0;
}

typedef struct TestApplyState
{
  float value;
  u32 writes;
  bool allow;
} TestApplyState;

static void *s_test_apply_resolve(
    void *user, LDKEditorUndoKey key, LDKFieldType type, size_t size)
{
  TestApplyState *state = (TestApplyState *)user;
  if (key.owner != 30 || key.property != 4 ||
      type != LDK_FIELD_FLOAT || size != sizeof(float))
  {
    return NULL;
  }
  return &state->value;
}

static bool s_test_apply_write(void *user, LDKEditorUndoKey key,
    LDKFieldType type, const void *value, size_t size)
{
  TestApplyState *state = (TestApplyState *)user;
  if (!state->allow || s_test_apply_resolve(user, key, type, size) == NULL)
  {
    return false;
  }
  memcpy(&state->value, value, sizeof(float));
  ++state->writes;
  return true;
}

static int test_undo_property_aware_restoration(void)
{
  TestApplyState state = {1.0f, 0, true};
  LDKEditorUndo undo;
  LDKEditorUndoKey key = {30, 4};
  ldk_editor_undo_initialize_with_apply(
      &undo, s_test_apply_resolve, s_test_apply_write, &state);
  ASSERT_TRUE(ldk_editor_undo_begin_change(
      &undo, key, LDK_FIELD_FLOAT, &state.value, sizeof(float)));
  state.value = 5.0f;
  ASSERT_TRUE(ldk_editor_undo_end_change_resolved(&undo));
  ASSERT_TRUE(ldk_editor_undo_undo(&undo));
  ASSERT_EQ(state.value, 1.0f);
  ASSERT_EQ(state.writes, 1);
  ASSERT_TRUE(ldk_editor_undo_redo(&undo));
  ASSERT_EQ(state.value, 5.0f);
  ASSERT_EQ(state.writes, 2);
  state.allow = false;
  ASSERT_FALSE(ldk_editor_undo_undo(&undo));
  ASSERT_EQ(undo.position, 1);
  ASSERT_EQ(state.value, 5.0f);
  ldk_editor_undo_terminate(&undo);
  return 0;
}

/* Materials are undoed by complete descriptor snapshots, not by pointers to
 * the temporary copies drawn by the Inspector. Both owner types share the
 * same value representation but have independent identities. */
typedef struct TestMaterialUndoState
{
  LDKMaterialDesc asset;
  LDKMaterialDesc inline_slot;
  bool asset_valid;
  bool slot_valid;
  u32 asset_updates;
  u32 slot_updates;
} TestMaterialUndoState;

static void *s_test_material_resolve(
    void *user, LDKEditorUndoKey key, LDKFieldType type, size_t size)
{
  TestMaterialUndoState *state = (TestMaterialUndoState *)user;
  if (type != LDK_FIELD_ASSET_MATERIAL || size != sizeof(LDKMaterialDesc))
  {
    return NULL;
  }
  if (key.owner == 100u && key.property == 1u && state->asset_valid)
  {
    return &state->asset;
  }
  if (key.owner == 200u && key.property == 2u && state->slot_valid)
  {
    return &state->inline_slot;
  }
  return NULL;
}

static bool s_test_material_apply(void *user, LDKEditorUndoKey key,
    LDKFieldType type, const void *value, size_t size)
{
  TestMaterialUndoState *state = (TestMaterialUndoState *)user;
  void *destination = s_test_material_resolve(user, key, type, size);
  if (!destination || !value)
  {
    return false;
  }
  memcpy(destination, value, size);
  if (key.owner == 100u)
  {
    ++state->asset_updates;
  }
  else
  {
    ++state->slot_updates;
  }
  return true;
}

static int test_undo_material_snapshots(void)
{
  TestMaterialUndoState state = {0};
  LDKEditorUndo undo;
  LDKEditorUndoKey asset = {100u, 1u};
  LDKEditorUndoKey inline_slot = {200u, 2u};
  state.asset_valid = true;
  state.slot_valid = true;
  state.asset.type = LDK_MATERIAL_TYPE_VERTEX_COLOR;
  state.asset.args.vertex_color.color = 0x112233FFu;
  state.asset.surface.specular = 0.1f;
  state.inline_slot.type = LDK_MATERIAL_TYPE_TEXTURED;

  ldk_editor_undo_initialize_with_apply(&undo,
      s_test_material_resolve, s_test_material_apply, &state);

  /* One color/slider drag produces one undo operation. */
  ASSERT_TRUE(ldk_editor_undo_begin_change(&undo, asset,
      LDK_FIELD_ASSET_MATERIAL, &state.asset, sizeof(state.asset)));
  state.asset.args.vertex_color.color = 0xAABBCC40u;
  state.asset.surface.specular = 0.5f;
  state.asset.surface.shininess = 16.0f;
  state.asset.surface.emission = 1.25f;
  state.asset.alpha_cutoff = 0.4f;
  ASSERT_TRUE(ldk_editor_undo_end_change_resolved(&undo));
  ASSERT_EQ(undo.count, 1u);
  ASSERT_TRUE(ldk_editor_undo_undo(&undo));
  ASSERT_EQ(state.asset.args.vertex_color.color, 0x112233FFu);
  ASSERT_EQ(state.asset.surface.specular, 0.1f);
  ASSERT_EQ(state.asset.surface.emission, 0.0f);
  ASSERT_TRUE(ldk_editor_undo_redo(&undo));
  ASSERT_EQ(state.asset.args.vertex_color.color, 0xAABBCC40u);
  ASSERT_EQ(state.asset.surface.emission, 1.25f);
  ASSERT_EQ(state.asset_updates, 2u);

  /* Add, assign, change settings, and remove a texture in an inline slot. */
  ASSERT_TRUE(ldk_editor_undo_begin_change(&undo, inline_slot,
      LDK_FIELD_ASSET_MATERIAL, &state.inline_slot, sizeof(state.inline_slot)));
  state.inline_slot.texture_slot_mask |= 1u << 3u;
  state.inline_slot.additional_textures[0].h.index = 42u;
  state.inline_slot.additional_textures[0].h.version = 1u;
  state.inline_slot.texture_settings[3].independent = true;
  state.inline_slot.texture_settings[3].filter = LDK_MATERIAL_TEXTURE_FILTER_LINEAR;
  ASSERT_TRUE(ldk_editor_undo_end_change_resolved(&undo));
  ASSERT_EQ(undo.count, 2u);
  ASSERT_TRUE(ldk_editor_undo_undo(&undo));
  ASSERT_EQ(state.inline_slot.texture_slot_mask, 0u);
  ASSERT_EQ(state.inline_slot.additional_textures[0].h.index, 0u);
  ASSERT_FALSE(state.inline_slot.texture_settings[3].independent);
  ASSERT_TRUE(ldk_editor_undo_redo(&undo));
  ASSERT_EQ(state.inline_slot.texture_slot_mask, 1u << 3u);
  ASSERT_EQ(state.inline_slot.additional_textures[0].h.index, 42u);
  ASSERT_TRUE(state.inline_slot.texture_settings[3].independent);
  ASSERT_EQ(state.slot_updates, 2u);

  /* Deleting the target must leave the history position unchanged. */
  state.slot_valid = false;
  ASSERT_FALSE(ldk_editor_undo_undo(&undo));
  ASSERT_EQ(undo.position, 2u);
  state.slot_valid = true;
  ASSERT_TRUE(ldk_editor_undo_undo(&undo));

  /* Removal is a separate operation, also undoable. */
  ASSERT_TRUE(ldk_editor_undo_redo(&undo));
  ASSERT_TRUE(ldk_editor_undo_begin_change(&undo, inline_slot,
      LDK_FIELD_ASSET_MATERIAL, &state.inline_slot, sizeof(state.inline_slot)));
  state.inline_slot.texture_slot_mask = 0u;
  ASSERT_TRUE(ldk_editor_undo_end_change_resolved(&undo));
  ASSERT_TRUE(ldk_editor_undo_undo(&undo));
  ASSERT_EQ(state.inline_slot.texture_slot_mask, 1u << 3u);
  ldk_editor_undo_terminate(&undo);
  return 0;
}

int main(void)
{
  STDXTestCase tests[] = {
    X_TEST(test_undo_lifecycle),
    X_TEST(test_undo_relocated_property),
    X_TEST(test_undo_composite_and_asset),
    X_TEST(test_undo_end_after_caller_write),
    X_TEST(test_undo_property_aware_restoration),
    X_TEST(test_undo_material_snapshots),
  };
  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
