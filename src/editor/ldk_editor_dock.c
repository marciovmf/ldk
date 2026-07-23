#include <stdio.h>
#include <string.h>

#ifndef LDK_EDITOR_DOCK_WORKSPACE_TOP
#define LDK_EDITOR_DOCK_WORKSPACE_TOP                                      \
  (LDK_UI_DEFAULT_CONTROL_HEIGHT * 2.0f + LDK_UI_DEFAULT_PADDING * 4.0f)
#endif

#ifndef LDK_EDITOR_DOCK_NODE_CAPACITY
#define LDK_EDITOR_DOCK_NODE_CAPACITY 32
#endif

#ifndef LDK_EDITOR_DOCK_TAB_HEIGHT
#define LDK_EDITOR_DOCK_TAB_HEIGHT 28.0f
#endif

#ifndef LDK_EDITOR_DOCK_SPLIT_GAP
#define LDK_EDITOR_DOCK_SPLIT_GAP 0.0f
#endif

#ifndef LDK_EDITOR_DOCK_DRAG_THRESHOLD
#define LDK_EDITOR_DOCK_DRAG_THRESHOLD 6.0f
#endif

#ifndef LDK_EDITOR_DOCK_SPLITTER_HIT_SIZE
#define LDK_EDITOR_DOCK_SPLITTER_HIT_SIZE 8.0f
#endif

#ifndef LDK_EDITOR_DOCK_TARGET_SIZE
#define LDK_EDITOR_DOCK_TARGET_SIZE 44.0f
#endif

#ifndef LDK_EDITOR_DOCK_TARGET_GAP
#define LDK_EDITOR_DOCK_TARGET_GAP 6.0f
#endif

#ifndef LDK_EDITOR_DOCK_PROJECT_ENTRY_LIMIT
#define LDK_EDITOR_DOCK_PROJECT_ENTRY_LIMIT 128
#endif

#define LDK_EDITOR_DOCK_INVALID_NODE (-1)

#define LDK_EDITOR_DOCK_TARGET_COLOR_IDLE 0xFFFFFFFF
#define LDK_EDITOR_DOCK_TARGET_COLOR_HOVER 0x0000FFFF

typedef enum LDKEditorDockWindowId
{
  LDK_EDITOR_DOCK_WINDOW_PROJECT_EXPLORER = 0,
  LDK_EDITOR_DOCK_WINDOW_SCENE,
  LDK_EDITOR_DOCK_WINDOW_INSPECTOR,
  LDK_EDITOR_DOCK_WINDOW_CONSOLE,
  LDK_EDITOR_DOCK_WINDOW_COUNT
} LDKEditorDockWindowId;

typedef enum LDKEditorDockNodeType
{
  LDK_EDITOR_DOCK_NODE_NONE = 0,
  LDK_EDITOR_DOCK_NODE_LEAF,
  LDK_EDITOR_DOCK_NODE_SPLIT
} LDKEditorDockNodeType;

typedef enum LDKEditorDockSplitAxis
{
  LDK_EDITOR_DOCK_SPLIT_HORIZONTAL = 0,
  LDK_EDITOR_DOCK_SPLIT_VERTICAL
} LDKEditorDockSplitAxis;

typedef enum LDKEditorDockTarget
{
  LDK_EDITOR_DOCK_TARGET_NONE = 0,

  LDK_EDITOR_DOCK_TARGET_ABSOLUTE_TOP,
  LDK_EDITOR_DOCK_TARGET_ABSOLUTE_LEFT,
  LDK_EDITOR_DOCK_TARGET_ABSOLUTE_RIGHT,
  LDK_EDITOR_DOCK_TARGET_ABSOLUTE_BOTTOM,

  LDK_EDITOR_DOCK_TARGET_LOCAL_TOP,
  LDK_EDITOR_DOCK_TARGET_LOCAL_LEFT,
  LDK_EDITOR_DOCK_TARGET_LOCAL_CENTER,
  LDK_EDITOR_DOCK_TARGET_LOCAL_RIGHT,
  LDK_EDITOR_DOCK_TARGET_LOCAL_BOTTOM
} LDKEditorDockTarget;

typedef struct LDKEditorDockWindow
{
  const char *title;
  LDKUIRect floating_rect;
  LDKUIRect tab_rect;
  LDKUIId ui_window_id;
  i32 leaf;
  bool tab_rect_valid;
} LDKEditorDockWindow;

typedef struct LDKEditorDockLeaf
{
  u32 windows[LDK_EDITOR_DOCK_WINDOW_COUNT];
  u32 window_count;
  u32 active_window;
} LDKEditorDockLeaf;

typedef struct LDKEditorDockSplit
{
  i32 first;
  i32 second;
  float ratio;
  LDKEditorDockSplitAxis axis;
} LDKEditorDockSplit;

typedef struct LDKEditorDockNode
{
  LDKEditorDockNodeType type;
  i32 parent;
  LDKUIRect rect;
  LDKUIRect splitter_rect;
  bool used;

  union
  {
    LDKEditorDockLeaf leaf;
    LDKEditorDockSplit split;
  } data;
} LDKEditorDockNode;

typedef struct LDKEditorDockDrag
{
  u32 window;
  i32 source_leaf;
  LDKUIPoint press_position;
  LDKEditorDockTarget target;
  i32 target_leaf;
  bool pending;
  bool active;
} LDKEditorDockDrag;

typedef struct LDKEditorDockResize
{
  i32 split;
  bool active;
} LDKEditorDockResize;

typedef struct LDKEditorDockState
{
  LDKEditorDockWindow windows[LDK_EDITOR_DOCK_WINDOW_COUNT];
  LDKEditorDockNode nodes[LDK_EDITOR_DOCK_NODE_CAPACITY];
  LDKEditorDockDrag drag;
  LDKEditorDockResize resize;

  LDKUIPoint project_scroll;
  LDKUIPoint console_scroll;
  LDKUIId target_overlay_window_id;
  i32 root;
  bool inspector_enabled;
  bool initialized;
} LDKEditorDockState;

static LDKEditorDockState s_editor_dock;

static bool s_editor_dock_rect_contains(
  const LDKUIRect *rect, float x, float y)
{
  if (rect == NULL)
  {
    return false;
  }

  return x >= rect->x && y >= rect->y &&
         x < rect->x + rect->w && y < rect->y + rect->h;
}

static float s_editor_dock_clampf(float value, float min_value, float max_value)
{
  if (value < min_value)
  {
    return min_value;
  }

  if (value > max_value)
  {
    return max_value;
  }

  return value;
}

static LDKUIPoint s_editor_dock_cursor_get(const LDKUIContext *ui)
{
  if (ui == NULL || ui->mouse == NULL)
  {
    return (LDKUIPoint){0};
  }

  LDKPoint cursor = ldk_os_mouse_cursor((LDKMouseState *)ui->mouse);
  return (LDKUIPoint){(float)cursor.x, (float)cursor.y};
}

static bool s_editor_dock_mouse_down(const LDKUIContext *ui)
{
  return ui != NULL && ui->mouse != NULL &&
         ldk_os_mouse_button_down(
           (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT);
}

static bool s_editor_dock_mouse_pressed(const LDKUIContext *ui)
{
  return ui != NULL && ui->mouse != NULL &&
         ldk_os_mouse_button_is_pressed(
           (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT);
}

static bool s_editor_dock_mouse_up(const LDKUIContext *ui)
{
  return ui != NULL && ui->mouse != NULL &&
         ldk_os_mouse_button_up(
           (LDKMouseState *)ui->mouse, LDK_MOUSE_BUTTON_LEFT);
}

static bool s_editor_dock_escape_down(const LDKUIContext *ui)
{
  return ui != NULL && ui->keyboard != NULL &&
         ldk_os_keyboard_key_down(
           (LDKKeyboardState *)ui->keyboard, LDK_KEYCODE_ESCAPE);
}

static LDKUIRect s_editor_dock_workspace_rect(const LDKUIContext *ui)
{
  if (ui == NULL)
  {
    return (LDKUIRect){0};
  }

  float height = ui->viewport.h - LDK_EDITOR_DOCK_WORKSPACE_TOP;
  if (height < 0.0f)
  {
    height = 0.0f;
  }

  return (LDKUIRect){
    ui->viewport.x,
    ui->viewport.y + LDK_EDITOR_DOCK_WORKSPACE_TOP,
    ui->viewport.w,
    height
  };
}

static void s_editor_dock_resize_reset(LDKEditorDockResize *resize)
{
  if (resize == NULL)
  {
    return;
  }

  *resize = (LDKEditorDockResize){
    .split = LDK_EDITOR_DOCK_INVALID_NODE
  };
}

static void s_editor_dock_drag_reset(LDKEditorDockDrag *drag)
{
  if (drag == NULL)
  {
    return;
  }

  *drag = (LDKEditorDockDrag){
    .window = LDK_EDITOR_DOCK_WINDOW_COUNT,
    .source_leaf = LDK_EDITOR_DOCK_INVALID_NODE,
    .target = LDK_EDITOR_DOCK_TARGET_NONE,
    .target_leaf = LDK_EDITOR_DOCK_INVALID_NODE
  };
}

static i32 s_editor_dock_node_allocate(LDKEditorDockState *dock)
{
  for (i32 i = 0; i < LDK_EDITOR_DOCK_NODE_CAPACITY; ++i)
  {
    if (!dock->nodes[i].used)
    {
      dock->nodes[i] = (LDKEditorDockNode){
        .type = LDK_EDITOR_DOCK_NODE_NONE,
        .parent = LDK_EDITOR_DOCK_INVALID_NODE,
        .used = true
      };
      return i;
    }
  }

  return LDK_EDITOR_DOCK_INVALID_NODE;
}

static void s_editor_dock_node_release(
  LDKEditorDockState *dock, i32 node_index)
{
  if (dock == NULL || node_index < 0 ||
      node_index >= LDK_EDITOR_DOCK_NODE_CAPACITY)
  {
    return;
  }

  dock->nodes[node_index] = (LDKEditorDockNode){
    .parent = LDK_EDITOR_DOCK_INVALID_NODE
  };
}

static i32 s_editor_dock_leaf_create(
  LDKEditorDockState *dock, u32 window)
{
  i32 node_index = s_editor_dock_node_allocate(dock);
  if (node_index == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return node_index;
  }

  LDKEditorDockNode *node = &dock->nodes[node_index];
  node->type = LDK_EDITOR_DOCK_NODE_LEAF;
  node->data.leaf.windows[0] = window;
  node->data.leaf.window_count = 1;
  node->data.leaf.active_window = window;
  return node_index;
}

static i32 s_editor_dock_leaf_create_tabs(
  LDKEditorDockState *dock, const u32 *windows, u32 window_count,
  u32 active_window)
{
  i32 node_index = s_editor_dock_node_allocate(dock);
  if (node_index == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return node_index;
  }

  LDKEditorDockNode *node = &dock->nodes[node_index];
  node->type = LDK_EDITOR_DOCK_NODE_LEAF;
  node->data.leaf.window_count = window_count;
  node->data.leaf.active_window = active_window;

  for (u32 i = 0; i < window_count; ++i)
  {
    node->data.leaf.windows[i] = windows[i];
  }

  return node_index;
}

static i32 s_editor_dock_split_create(LDKEditorDockState *dock,
  LDKEditorDockSplitAxis axis, float ratio, i32 first, i32 second)
{
  i32 node_index = s_editor_dock_node_allocate(dock);
  if (node_index == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return node_index;
  }

  LDKEditorDockNode *node = &dock->nodes[node_index];
  node->type = LDK_EDITOR_DOCK_NODE_SPLIT;
  node->data.split.axis = axis;
  node->data.split.ratio = ratio;
  node->data.split.first = first;
  node->data.split.second = second;

  dock->nodes[first].parent = node_index;
  dock->nodes[second].parent = node_index;
  return node_index;
}

static bool s_editor_dock_leaf_contains(
  const LDKEditorDockLeaf *leaf, u32 window)
{
  if (leaf == NULL)
  {
    return false;
  }

  for (u32 i = 0; i < leaf->window_count; ++i)
  {
    if (leaf->windows[i] == window)
    {
      return true;
    }
  }

  return false;
}

static bool s_editor_dock_leaf_add(
  LDKEditorDockLeaf *leaf, u32 window)
{
  if (leaf == NULL || window >= LDK_EDITOR_DOCK_WINDOW_COUNT)
  {
    return false;
  }

  if (s_editor_dock_leaf_contains(leaf, window))
  {
    leaf->active_window = window;
    return true;
  }

  if (leaf->window_count >= LDK_EDITOR_DOCK_WINDOW_COUNT)
  {
    return false;
  }

  leaf->windows[leaf->window_count++] = window;
  leaf->active_window = window;
  return true;
}

static bool s_editor_dock_leaf_remove(
  LDKEditorDockLeaf *leaf, u32 window)
{
  if (leaf == NULL)
  {
    return false;
  }

  for (u32 i = 0; i < leaf->window_count; ++i)
  {
    if (leaf->windows[i] != window)
    {
      continue;
    }

    for (u32 j = i + 1; j < leaf->window_count; ++j)
    {
      leaf->windows[j - 1] = leaf->windows[j];
    }

    leaf->window_count -= 1;

    if (leaf->window_count > 0 && leaf->active_window == window)
    {
      leaf->active_window = leaf->windows[0];
    }

    return true;
  }

  return false;
}

static void s_editor_dock_replace_child(LDKEditorDockState *dock,
  i32 parent_index, i32 old_child, i32 new_child)
{
  if (parent_index == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    dock->root = new_child;
    if (new_child != LDK_EDITOR_DOCK_INVALID_NODE)
    {
      dock->nodes[new_child].parent = LDK_EDITOR_DOCK_INVALID_NODE;
    }
    return;
  }

  LDKEditorDockSplit *split = &dock->nodes[parent_index].data.split;
  if (split->first == old_child)
  {
    split->first = new_child;
  }
  else if (split->second == old_child)
  {
    split->second = new_child;
  }

  if (new_child != LDK_EDITOR_DOCK_INVALID_NODE)
  {
    dock->nodes[new_child].parent = parent_index;
  }
}

static void s_editor_dock_empty_leaf_collapse(
  LDKEditorDockState *dock, i32 leaf_index)
{
  LDKEditorDockNode *leaf = &dock->nodes[leaf_index];
  if (leaf->type != LDK_EDITOR_DOCK_NODE_LEAF ||
      leaf->data.leaf.window_count != 0)
  {
    return;
  }

  i32 parent_index = leaf->parent;
  if (parent_index == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    s_editor_dock_node_release(dock, leaf_index);
    dock->root = LDK_EDITOR_DOCK_INVALID_NODE;
    return;
  }

  LDKEditorDockNode *parent = &dock->nodes[parent_index];
  i32 sibling_index = parent->data.split.first == leaf_index
                        ? parent->data.split.second
                        : parent->data.split.first;
  i32 grandparent_index = parent->parent;

  s_editor_dock_replace_child(
    dock, grandparent_index, parent_index, sibling_index);
  s_editor_dock_node_release(dock, leaf_index);
  s_editor_dock_node_release(dock, parent_index);
}

static void s_editor_dock_window_locations_refresh(LDKEditorDockState *dock)
{
  for (u32 i = 0; i < LDK_EDITOR_DOCK_WINDOW_COUNT; ++i)
  {
    dock->windows[i].leaf = LDK_EDITOR_DOCK_INVALID_NODE;
  }

  for (i32 node_index = 0;
       node_index < LDK_EDITOR_DOCK_NODE_CAPACITY;
       ++node_index)
  {
    LDKEditorDockNode *node = &dock->nodes[node_index];
    if (!node->used || node->type != LDK_EDITOR_DOCK_NODE_LEAF)
    {
      continue;
    }

    for (u32 window_index = 0;
         window_index < node->data.leaf.window_count;
         ++window_index)
    {
      u32 window = node->data.leaf.windows[window_index];
      dock->windows[window].leaf = node_index;
    }
  }
}

static bool s_editor_dock_window_detach(
  LDKEditorDockState *dock, u32 window)
{
  if (window >= LDK_EDITOR_DOCK_WINDOW_COUNT)
  {
    return false;
  }

  i32 leaf_index = dock->windows[window].leaf;
  if (leaf_index == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return true;
  }

  LDKEditorDockNode *leaf = &dock->nodes[leaf_index];
  if (!s_editor_dock_leaf_remove(&leaf->data.leaf, window))
  {
    return false;
  }

  if (leaf->data.leaf.window_count == 0)
  {
    s_editor_dock_empty_leaf_collapse(dock, leaf_index);
  }

  s_editor_dock_window_locations_refresh(dock);
  return true;
}

static bool s_editor_dock_local_center(
  LDKEditorDockState *dock, u32 window, i32 target_leaf)
{
  if (target_leaf < 0 || target_leaf >= LDK_EDITOR_DOCK_NODE_CAPACITY ||
      !dock->nodes[target_leaf].used ||
      dock->nodes[target_leaf].type != LDK_EDITOR_DOCK_NODE_LEAF)
  {
    return false;
  }

  if (dock->windows[window].leaf == target_leaf)
  {
    dock->nodes[target_leaf].data.leaf.active_window = window;
    return true;
  }

  if (!s_editor_dock_window_detach(dock, window))
  {
    return false;
  }

  if (!dock->nodes[target_leaf].used ||
      dock->nodes[target_leaf].type != LDK_EDITOR_DOCK_NODE_LEAF)
  {
    return false;
  }

  if (!s_editor_dock_leaf_add(&dock->nodes[target_leaf].data.leaf, window))
  {
    return false;
  }

  s_editor_dock_window_locations_refresh(dock);
  return true;
}

static bool s_editor_dock_local_edge(LDKEditorDockState *dock, u32 window,
  i32 target_leaf, LDKEditorDockTarget target)
{
  if (target_leaf < 0 || target_leaf >= LDK_EDITOR_DOCK_NODE_CAPACITY ||
      !dock->nodes[target_leaf].used ||
      dock->nodes[target_leaf].type != LDK_EDITOR_DOCK_NODE_LEAF)
  {
    return false;
  }

  i32 source_leaf = dock->windows[window].leaf;
  if (source_leaf == target_leaf &&
      dock->nodes[target_leaf].data.leaf.window_count == 1)
  {
    return true;
  }

  if (!s_editor_dock_window_detach(dock, window))
  {
    return false;
  }

  if (!dock->nodes[target_leaf].used ||
      dock->nodes[target_leaf].type != LDK_EDITOR_DOCK_NODE_LEAF)
  {
    return false;
  }

  i32 new_leaf = s_editor_dock_leaf_create(dock, window);
  if (new_leaf == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return false;
  }

  i32 old_parent = dock->nodes[target_leaf].parent;
  LDKEditorDockSplitAxis axis =
    target == LDK_EDITOR_DOCK_TARGET_LOCAL_LEFT ||
    target == LDK_EDITOR_DOCK_TARGET_LOCAL_RIGHT
      ? LDK_EDITOR_DOCK_SPLIT_HORIZONTAL
      : LDK_EDITOR_DOCK_SPLIT_VERTICAL;
  bool new_first = target == LDK_EDITOR_DOCK_TARGET_LOCAL_LEFT ||
                   target == LDK_EDITOR_DOCK_TARGET_LOCAL_TOP;
  i32 first = new_first ? new_leaf : target_leaf;
  i32 second = new_first ? target_leaf : new_leaf;
  i32 split = s_editor_dock_split_create(dock, axis, 0.5f, first, second);

  if (split == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    s_editor_dock_node_release(dock, new_leaf);
    return false;
  }

  dock->nodes[split].parent = old_parent;
  s_editor_dock_replace_child(dock, old_parent, target_leaf, split);
  s_editor_dock_window_locations_refresh(dock);
  return true;
}

static bool s_editor_dock_absolute_edge(LDKEditorDockState *dock, u32 window,
  LDKEditorDockTarget target)
{
  if (!s_editor_dock_window_detach(dock, window))
  {
    return false;
  }

  i32 new_leaf = s_editor_dock_leaf_create(dock, window);
  if (new_leaf == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return false;
  }

  if (dock->root == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    dock->root = new_leaf;
    dock->nodes[new_leaf].parent = LDK_EDITOR_DOCK_INVALID_NODE;
    s_editor_dock_window_locations_refresh(dock);
    return true;
  }

  i32 old_root = dock->root;
  LDKEditorDockSplitAxis axis =
    target == LDK_EDITOR_DOCK_TARGET_ABSOLUTE_LEFT ||
    target == LDK_EDITOR_DOCK_TARGET_ABSOLUTE_RIGHT
      ? LDK_EDITOR_DOCK_SPLIT_HORIZONTAL
      : LDK_EDITOR_DOCK_SPLIT_VERTICAL;
  bool new_first = target == LDK_EDITOR_DOCK_TARGET_ABSOLUTE_LEFT ||
                   target == LDK_EDITOR_DOCK_TARGET_ABSOLUTE_TOP;
  i32 first = new_first ? new_leaf : old_root;
  i32 second = new_first ? old_root : new_leaf;
  i32 split = s_editor_dock_split_create(dock, axis, 0.25f, first, second);

  if (split == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    s_editor_dock_node_release(dock, new_leaf);
    return false;
  }

  dock->root = split;
  dock->nodes[split].parent = LDK_EDITOR_DOCK_INVALID_NODE;
  s_editor_dock_window_locations_refresh(dock);
  return true;
}

static void s_editor_dock_make_floating(LDKEditorDockState *dock,
  u32 window, LDKUIPoint cursor)
{
  if (window >= LDK_EDITOR_DOCK_WINDOW_COUNT)
  {
    return;
  }

  bool was_docked = dock->windows[window].leaf != LDK_EDITOR_DOCK_INVALID_NODE;
  if (was_docked && !s_editor_dock_window_detach(dock, window))
  {
    return;
  }

  if (was_docked)
  {
    LDKUIRect *rect = &dock->windows[window].floating_rect;
    rect->x = cursor.x - rect->w * 0.5f;
    rect->y = cursor.y - LDK_UI_TITLE_BAR_HEIGHT * 0.5f;
  }
}

static void s_editor_dock_drop_commit(LDKEditorDockState *dock,
  LDKEditorDockDrag *drag, LDKUIPoint cursor)
{
  if (dock == NULL || drag == NULL ||
      drag->window >= LDK_EDITOR_DOCK_WINDOW_COUNT)
  {
    return;
  }

  switch (drag->target)
  {
    case LDK_EDITOR_DOCK_TARGET_LOCAL_CENTER:
      s_editor_dock_local_center(dock, drag->window, drag->target_leaf);
      break;

    case LDK_EDITOR_DOCK_TARGET_LOCAL_TOP:
    case LDK_EDITOR_DOCK_TARGET_LOCAL_LEFT:
    case LDK_EDITOR_DOCK_TARGET_LOCAL_RIGHT:
    case LDK_EDITOR_DOCK_TARGET_LOCAL_BOTTOM:
      s_editor_dock_local_edge(
        dock, drag->window, drag->target_leaf, drag->target);
      break;

    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_TOP:
    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_LEFT:
    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_RIGHT:
    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_BOTTOM:
      s_editor_dock_absolute_edge(dock, drag->window, drag->target);
      break;

    case LDK_EDITOR_DOCK_TARGET_NONE:
    default:
      s_editor_dock_make_floating(dock, drag->window, cursor);
      break;
  }
}

static void s_editor_dock_layout_node(
  LDKEditorDockState *dock, i32 node_index, LDKUIRect rect)
{
  if (node_index < 0 || node_index >= LDK_EDITOR_DOCK_NODE_CAPACITY ||
      !dock->nodes[node_index].used)
  {
    return;
  }

  LDKEditorDockNode *node = &dock->nodes[node_index];
  node->rect = rect;
  node->splitter_rect = (LDKUIRect){0};

  if (node->type != LDK_EDITOR_DOCK_NODE_SPLIT)
  {
    return;
  }

  float ratio = s_editor_dock_clampf(node->data.split.ratio, 0.1f, 0.9f);
  float half_gap = LDK_EDITOR_DOCK_SPLIT_GAP * 0.5f;
  float half_hit_size = LDK_EDITOR_DOCK_SPLITTER_HIT_SIZE * 0.5f;
  LDKUIRect first_rect = rect;
  LDKUIRect second_rect = rect;

  node->data.split.ratio = ratio;

  if (node->data.split.axis == LDK_EDITOR_DOCK_SPLIT_HORIZONTAL)
  {
    float split_x = rect.w * ratio;
    first_rect.w = split_x - half_gap;
    second_rect.x = rect.x + split_x + half_gap;
    second_rect.w = rect.w - split_x - half_gap;
    node->splitter_rect = (LDKUIRect){
      rect.x + split_x - half_hit_size,
      rect.y,
      LDK_EDITOR_DOCK_SPLITTER_HIT_SIZE,
      rect.h
    };
  }
  else
  {
    float split_y = rect.h * ratio;
    first_rect.h = split_y - half_gap;
    second_rect.y = rect.y + split_y + half_gap;
    second_rect.h = rect.h - split_y - half_gap;
    node->splitter_rect = (LDKUIRect){
      rect.x,
      rect.y + split_y - half_hit_size,
      rect.w,
      LDK_EDITOR_DOCK_SPLITTER_HIT_SIZE
    };
  }

  s_editor_dock_layout_node(
    dock, node->data.split.first, first_rect);
  s_editor_dock_layout_node(
    dock, node->data.split.second, second_rect);
}

static i32 s_editor_dock_split_at(
  LDKEditorDockState *dock, LDKUIPoint cursor)
{
  for (i32 i = 0; i < LDK_EDITOR_DOCK_NODE_CAPACITY; ++i)
  {
    LDKEditorDockNode *node = &dock->nodes[i];
    if (!node->used || node->type != LDK_EDITOR_DOCK_NODE_SPLIT)
    {
      continue;
    }

    if (s_editor_dock_rect_contains(
          &node->splitter_rect, cursor.x, cursor.y))
    {
      return i;
    }
  }

  return LDK_EDITOR_DOCK_INVALID_NODE;
}

static bool s_editor_dock_split_resize_update(
  LDKEditorDockState *dock, LDKUIContext *ui)
{
  if (dock == NULL || ui == NULL)
  {
    return false;
  }

  LDKUIPoint cursor = s_editor_dock_cursor_get(ui);

  if (dock->resize.active)
  {
    i32 split_index = dock->resize.split;
    if (split_index < 0 ||
        split_index >= LDK_EDITOR_DOCK_NODE_CAPACITY ||
        !dock->nodes[split_index].used ||
        dock->nodes[split_index].type != LDK_EDITOR_DOCK_NODE_SPLIT)
    {
      s_editor_dock_resize_reset(&dock->resize);
      return false;
    }

    LDKEditorDockNode *node = &dock->nodes[split_index];
    if (node->data.split.axis == LDK_EDITOR_DOCK_SPLIT_HORIZONTAL)
    {
      ui->cursor_type = LDK_CURSOR_SIZE_WE;
    }
    else
    {
      ui->cursor_type = LDK_CURSOR_SIZE_NS;
    }

    if (s_editor_dock_mouse_pressed(ui))
    {
      float position = node->data.split.axis ==
                         LDK_EDITOR_DOCK_SPLIT_HORIZONTAL
                         ? cursor.x - node->rect.x
                         : cursor.y - node->rect.y;
      float size = node->data.split.axis ==
                     LDK_EDITOR_DOCK_SPLIT_HORIZONTAL
                     ? node->rect.w
                     : node->rect.h;

      if (size > 0.0f)
      {
        node->data.split.ratio =
          s_editor_dock_clampf(position / size, 0.1f, 0.9f);
      }

      return true;
    }

    if (s_editor_dock_mouse_up(ui))
    {
      s_editor_dock_resize_reset(&dock->resize);
    }

    return true;
  }

  if (dock->drag.pending || dock->drag.active)
  {
    return false;
  }

  i32 split_index = s_editor_dock_split_at(dock, cursor);
  if (split_index == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return false;
  }

  LDKEditorDockNode *node = &dock->nodes[split_index];
  if (node->data.split.axis == LDK_EDITOR_DOCK_SPLIT_HORIZONTAL)
  {
    ui->cursor_type = LDK_CURSOR_SIZE_WE;
  }
  else
  {
    ui->cursor_type = LDK_CURSOR_SIZE_NS;
  }

  if (s_editor_dock_mouse_down(ui))
  {
    dock->resize.split = split_index;
    dock->resize.active = true;
  }

  return true;
}

static i32 s_editor_dock_leaf_at(
  LDKEditorDockState *dock, LDKUIPoint cursor)
{
  for (i32 i = 0; i < LDK_EDITOR_DOCK_NODE_CAPACITY; ++i)
  {
    LDKEditorDockNode *node = &dock->nodes[i];
    if (!node->used || node->type != LDK_EDITOR_DOCK_NODE_LEAF)
    {
      continue;
    }

    if (s_editor_dock_rect_contains(&node->rect, cursor.x, cursor.y))
    {
      return i;
    }
  }

  return LDK_EDITOR_DOCK_INVALID_NODE;
}

static void s_editor_dock_project_explorer_draw(
  LDKEditorDockState *dock, LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;
  const XFSPath *root = NULL;

  if (editor->project.loaded && editor->project.project_root_path.length > 0)
  {
    root = &editor->project.project_root_path;
  }
  else if (editor->engine_runtree.length > 0)
  {
    root = &editor->engine_runtree;
  }

  if (root == NULL)
  {
    ldk_ui_label(ui, "No project root is available.");
    return;
  }

  ldk_ui_label(ui, root->buf);
  dock->project_scroll = ldk_ui_begin_scrollview(ui, dock->project_scroll,
    LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  XFSDireEntry entry = {0};
  XFSDireHandle *directory =
    x_fs_find_first_file(x_fs_path_cstr(root), &entry);
  u32 entry_index = 0;

  while (directory != NULL && entry_index < LDK_EDITOR_DOCK_PROJECT_ENTRY_LIMIT)
  {
    bool special = strcmp(entry.name, ".") == 0 ||
                   strcmp(entry.name, "..") == 0;

    if (!special)
    {
      char label[256];
      snprintf(label, sizeof(label), "%s %.248s",
        entry.is_directory ? "[D]" : "[F]", entry.name);

      ldk_ui_push_id_u32(ui, entry_index++);
      ldk_ui_button_flat(ui, label);
      ldk_ui_pop_id(ui);
    }

    if (!x_fs_find_next_file(directory, &entry))
    {
      break;
    }
  }

  if (directory != NULL)
  {
    x_fs_find_close(directory);
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
}

static void s_editor_dock_scene_draw(LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;
  ldk_ui_label(ui, "Scene viewport placeholder");
  ldk_ui_horizontal_line(ui);
  ldk_ui_label(ui, "The docking POC intentionally keeps rendering simple.");
  ldk_ui_button(ui, "Frame selected");
  ldk_ui_button(ui, "Toggle grid");
  ldk_ui_spacer(ui);
}

static void s_editor_dock_inspector_draw(
  LDKEditorDockState *dock, LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;
  ldk_ui_label(ui, "Inspector placeholder");
  ldk_ui_horizontal_line(ui);
  ldk_ui_label(ui, "Name: Selected Entity");
  dock->inspector_enabled =
    ldk_ui_toggle(ui, dock->inspector_enabled);
  ldk_ui_label(ui, dock->inspector_enabled ? "Enabled" : "Disabled");
  ldk_ui_button(ui, "Add Component");
  ldk_ui_spacer(ui);
}

static void s_editor_dock_console_draw(
  LDKEditorDockState *dock, LDKEditorContext *editor)
{
  LDKUIContext *ui = &editor->ui;
  dock->console_scroll = ldk_ui_begin_scrollview(ui, dock->console_scroll,
    LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  if (editor->console_sb != NULL)
  {
    ldk_ui_label(ui, x_strbuilder_to_string(editor->console_sb));
  }
  else
  {
    ldk_ui_label(ui, "Console is not initialized.");
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
}

static void s_editor_dock_window_content_draw(LDKEditorDockState *dock,
  LDKEditorContext *editor, u32 window)
{
  switch (window)
  {
    case LDK_EDITOR_DOCK_WINDOW_PROJECT_EXPLORER:
      s_editor_dock_project_explorer_draw(dock, editor);
      break;

    case LDK_EDITOR_DOCK_WINDOW_SCENE:
      s_editor_dock_scene_draw(editor);
      break;

    case LDK_EDITOR_DOCK_WINDOW_INSPECTOR:
      s_editor_dock_inspector_draw(dock, editor);
      break;

    case LDK_EDITOR_DOCK_WINDOW_CONSOLE:
      s_editor_dock_console_draw(dock, editor);
      break;

    default:
      break;
  }
}

static float s_editor_dock_tab_width(
  LDKUIContext *ui, const LDKUITabBarItem *item)
{
  float width = LDK_UI_DEFAULT_SPACING * 4.0f;
  bool has_icon = item != NULL && item->icon.texture != 0;
  bool has_label =
    item != NULL && item->label != NULL && item->label[0] != '\0';

  if (has_icon)
  {
    width += item->icon.size.w;
  }

  if (has_label)
  {
    LDKTextSize text_size =
      ldk_ttf_measure_text_cstr(ui->font, item->label);

    if (has_icon)
    {
      width += LDK_UI_DEFAULT_SPACING;
    }

    width += text_size.w;
  }

  return width;
}

static void s_editor_dock_leaf_draw(LDKEditorDockState *dock,
  LDKEditorContext *editor, i32 leaf_index)
{
  LDKEditorDockNode *node = &dock->nodes[leaf_index];
  LDKEditorDockLeaf *leaf = &node->data.leaf;
  if (leaf->window_count == 0)
  {
    return;
  }

  if (!s_editor_dock_leaf_contains(leaf, leaf->active_window))
  {
    leaf->active_window = leaf->windows[0];
  }

  LDKUIContext *ui = &editor->ui;
  LDKEditorDockWindow *active = &dock->windows[leaf->active_window];
  char window_title[32];
  snprintf(window_title, sizeof(window_title), "Dock Leaf %d", leaf_index);
  ldk_ui_begin_window_fixed(ui, window_title, node->rect,
    LDK_UI_WINDOW_BORDER | LDK_UI_WINDOW_NO_PADDING);
  active->ui_window_id = ui->last_id;

  LDKUITabBarItem tab_items[LDK_EDITOR_DOCK_WINDOW_COUNT] = {0};
  u32 active_index = 0;

  for (u32 i = 0; i < leaf->window_count; ++i)
  {
    u32 window = leaf->windows[i];

    tab_items[i] = (LDKUITabBarItem){
      .id = window + 1,
      .label = dock->windows[window].title
    };

    if (window == leaf->active_window)
    {
      active_index = i;
    }
  }

  LDKUITabBarResult tab_result =
    ldk_ui_tab_bar(ui, tab_items, leaf->window_count, active_index);
  LDKUIRect tab_bar_rect = ldk_ui_last_rect(ui);

  if (tab_result.active_index < leaf->window_count)
  {
    leaf->active_window = leaf->windows[tab_result.active_index];
  }

  LDKUIIcon menu_icon =
    ui->theme.icons[LDK_UI_THEME_ICON_MORE_VERT];
  float menu_button_width = LDK_UI_DEFAULT_SPACING;

  if (menu_icon.texture != 0)
  {
    menu_button_width += menu_icon.size.w / 2.0f;
  }

  float tab_x =
    tab_bar_rect.x + menu_button_width + LDK_UI_TAB_BAR_SPACING;
  float tab_bar_right = tab_bar_rect.x + tab_bar_rect.w;

  for (u32 i = 0; i < leaf->window_count; ++i)
  {
    u32 window = leaf->windows[i];
    float tab_width =
      s_editor_dock_tab_width(ui, &tab_items[i]);

    if (tab_x >= tab_bar_right)
    {
      break;
    }

    dock->windows[window].tab_rect = (LDKUIRect){
      tab_x,
      tab_bar_rect.y,
      s_editor_dock_clampf(
        tab_width, 0.0f, tab_bar_right - tab_x),
      LDK_UI_TAB_BAR_TAB_HEIGHT
    };
    dock->windows[window].tab_rect_valid = true;
    tab_x += tab_width + LDK_UI_TAB_BAR_SPACING;
  }
  s_editor_dock_window_content_draw(dock, editor, leaf->active_window);
  ldk_ui_end_window(ui);
}

static void s_editor_dock_floating_window_draw(LDKEditorDockState *dock,
  LDKEditorContext *editor, u32 window)
{
  LDKEditorDockWindow *panel = &dock->windows[window];
  LDKUIContext *ui = &editor->ui;
  u32 flags = LDK_UI_WINDOW_TITLE_BAR |
              LDK_UI_WINDOW_DRAGGABLE |
              LDK_UI_WINDOW_RESIZABLE |
              LDK_UI_WINDOW_BORDER;

  panel->floating_rect = ldk_ui_begin_window(
    ui, panel->title, panel->floating_rect, flags);
  panel->ui_window_id = ui->last_id;
  s_editor_dock_window_content_draw(dock, editor, window);
  ldk_ui_end_window(ui);
}

static void s_editor_dock_windows_draw(
  LDKEditorDockState *dock, LDKEditorContext *editor)
{
  for (u32 i = 0; i < LDK_EDITOR_DOCK_WINDOW_COUNT; ++i)
  {
    dock->windows[i].tab_rect_valid = false;
  }

  for (i32 node_index = 0;
       node_index < LDK_EDITOR_DOCK_NODE_CAPACITY;
       ++node_index)
  {
    LDKEditorDockNode *node = &dock->nodes[node_index];
    if (node->used && node->type == LDK_EDITOR_DOCK_NODE_LEAF)
    {
      s_editor_dock_leaf_draw(dock, editor, node_index);
    }
  }

  for (u32 window = 0; window < LDK_EDITOR_DOCK_WINDOW_COUNT; ++window)
  {
    if (dock->windows[window].leaf == LDK_EDITOR_DOCK_INVALID_NODE)
    {
      s_editor_dock_floating_window_draw(dock, editor, window);
    }
  }
}

static void s_editor_dock_tab_drag_update(
  LDKEditorDockState *dock, LDKUIContext *ui)
{
  LDKUIPoint cursor = s_editor_dock_cursor_get(ui);

  if (!dock->drag.pending && !dock->drag.active &&
      s_editor_dock_mouse_down(ui))
  {
    for (u32 window = 0; window < LDK_EDITOR_DOCK_WINDOW_COUNT; ++window)
    {
      LDKEditorDockWindow *panel = &dock->windows[window];
      if (!panel->tab_rect_valid ||
          !s_editor_dock_rect_contains(&panel->tab_rect, cursor.x, cursor.y))
      {
        continue;
      }

      dock->drag.pending = true;
      dock->drag.window = window;
      dock->drag.source_leaf = panel->leaf;
      dock->drag.press_position = cursor;
      return;
    }
  }

  if (!dock->drag.pending)
  {
    return;
  }

  if (s_editor_dock_mouse_pressed(ui))
  {
    float dx = cursor.x - dock->drag.press_position.x;
    float dy = cursor.y - dock->drag.press_position.y;
    float threshold = LDK_EDITOR_DOCK_DRAG_THRESHOLD;

    if (dx * dx + dy * dy >= threshold * threshold)
    {
      dock->drag.pending = false;
      dock->drag.active = true;
    }
    return;
  }

  if (s_editor_dock_mouse_up(ui))
  {
    u32 window = dock->drag.window;
    i32 leaf = dock->windows[window].leaf;
    if (leaf != LDK_EDITOR_DOCK_INVALID_NODE &&
        dock->nodes[leaf].used &&
        dock->nodes[leaf].type == LDK_EDITOR_DOCK_NODE_LEAF)
    {
      dock->nodes[leaf].data.leaf.active_window = window;
    }
  }

  s_editor_dock_drag_reset(&dock->drag);
}

static void s_editor_dock_floating_drag_update(
  LDKEditorDockState *dock, LDKUIContext *ui)
{
  if (dock->drag.active || dock->drag.pending)
  {
    return;
  }

  if (ui->dragging_window_id == 0)
  {
    return;
  }

  for (u32 window = 0; window < LDK_EDITOR_DOCK_WINDOW_COUNT; ++window)
  {
    LDKEditorDockWindow *panel = &dock->windows[window];
    if (panel->leaf == LDK_EDITOR_DOCK_INVALID_NODE &&
        panel->ui_window_id == ui->dragging_window_id)
    {
      dock->drag.window = window;
      dock->drag.source_leaf = LDK_EDITOR_DOCK_INVALID_NODE;
      dock->drag.active = true;
      return;
    }
  }
}

static LDKUIRect s_editor_dock_target_rect_absolute(
  LDKUIRect workspace, LDKEditorDockTarget target)
{
  float size = LDK_EDITOR_DOCK_TARGET_SIZE;
  float margin = 14.0f;
  float center_x = workspace.x + workspace.w * 0.5f;
  float center_y = workspace.y + workspace.h * 0.5f;

  switch (target)
  {
    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_TOP:
      return (LDKUIRect){center_x - size * 0.5f,
        workspace.y + margin, size, size};

    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_LEFT:
      return (LDKUIRect){workspace.x + margin,
        center_y - size * 0.5f, size, size};

    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_RIGHT:
      return (LDKUIRect){workspace.x + workspace.w - margin - size,
        center_y - size * 0.5f, size, size};

    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_BOTTOM:
      return (LDKUIRect){center_x - size * 0.5f,
        workspace.y + workspace.h - margin - size, size, size};

    default:
      return (LDKUIRect){0};
  }
}

static LDKUIRect s_editor_dock_target_rect_local(
  LDKUIRect leaf, LDKEditorDockTarget target)
{
  float size = LDK_EDITOR_DOCK_TARGET_SIZE;
  float step = size + LDK_EDITOR_DOCK_TARGET_GAP;
  float center_x = leaf.x + leaf.w * 0.5f;
  float center_y = leaf.y + leaf.h * 0.5f;
  LDKUIRect center = {
    center_x - size * 0.5f,
    center_y - size * 0.5f,
    size,
    size
  };

  switch (target)
  {
    case LDK_EDITOR_DOCK_TARGET_LOCAL_TOP:
      center.y -= step;
      break;

    case LDK_EDITOR_DOCK_TARGET_LOCAL_LEFT:
      center.x -= step;
      break;

    case LDK_EDITOR_DOCK_TARGET_LOCAL_RIGHT:
      center.x += step;
      break;

    case LDK_EDITOR_DOCK_TARGET_LOCAL_BOTTOM:
      center.y += step;
      break;

    case LDK_EDITOR_DOCK_TARGET_LOCAL_CENTER:
    default:
      break;
  }

  return center;
}

static const LDKEditorIcon s_editor_dock_target_icon(LDKEditorDockTarget target)
{
  switch (target)
  {
  case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_TOP: return LDK_EDITOR_ICON_DOCK_TOP;
  case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_LEFT:  return LDK_EDITOR_ICON_DOCK_LEFT;
  case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_RIGHT: return LDK_EDITOR_ICON_DOCK_RIGHT;
  case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_BOTTOM: return LDK_EDITOR_ICON_DOCK_BOTTOM;
  case LDK_EDITOR_DOCK_TARGET_LOCAL_TOP: return LDK_EDITOR_ICON_DOCK_TOP;
  case LDK_EDITOR_DOCK_TARGET_LOCAL_LEFT: return LDK_EDITOR_ICON_DOCK_LEFT;
  case LDK_EDITOR_DOCK_TARGET_LOCAL_CENTER: return LDK_EDITOR_ICON_DOCK_CENTER;
  case LDK_EDITOR_DOCK_TARGET_LOCAL_RIGHT: return LDK_EDITOR_ICON_DOCK_RIGHT;
  case LDK_EDITOR_DOCK_TARGET_LOCAL_BOTTOM: return LDK_EDITOR_ICON_DOCK_BOTTOM;
  default: return -1;
  }
}


static const char *s_editor_dock_target_label(LDKEditorDockTarget target)
{
  switch (target)
  {
    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_TOP: return "A^";
    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_LEFT: return "A<";
    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_RIGHT: return "A>";
    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_BOTTOM: return "Av";
    case LDK_EDITOR_DOCK_TARGET_LOCAL_TOP: return "^";
    case LDK_EDITOR_DOCK_TARGET_LOCAL_LEFT: return "<";
    case LDK_EDITOR_DOCK_TARGET_LOCAL_CENTER: return "C";
    case LDK_EDITOR_DOCK_TARGET_LOCAL_RIGHT: return ">";
    case LDK_EDITOR_DOCK_TARGET_LOCAL_BOTTOM: return "v";
    default: return "";
  }
}

static void s_editor_dock_target_evaluate(LDKEditorDockState *dock,
  LDKUIRect workspace, LDKUIPoint cursor)
{
  static const LDKEditorDockTarget absolute_targets[] = {
    LDK_EDITOR_DOCK_TARGET_ABSOLUTE_TOP,
    LDK_EDITOR_DOCK_TARGET_ABSOLUTE_LEFT,
    LDK_EDITOR_DOCK_TARGET_ABSOLUTE_RIGHT,
    LDK_EDITOR_DOCK_TARGET_ABSOLUTE_BOTTOM
  };
  static const LDKEditorDockTarget local_targets[] = {
    LDK_EDITOR_DOCK_TARGET_LOCAL_TOP,
    LDK_EDITOR_DOCK_TARGET_LOCAL_LEFT,
    LDK_EDITOR_DOCK_TARGET_LOCAL_CENTER,
    LDK_EDITOR_DOCK_TARGET_LOCAL_RIGHT,
    LDK_EDITOR_DOCK_TARGET_LOCAL_BOTTOM
  };

  dock->drag.target = LDK_EDITOR_DOCK_TARGET_NONE;
  dock->drag.target_leaf = s_editor_dock_leaf_at(dock, cursor);

  for (u32 i = 0;
       i < sizeof(absolute_targets) / sizeof(absolute_targets[0]);
       ++i)
  {
    LDKUIRect rect =
      s_editor_dock_target_rect_absolute(workspace, absolute_targets[i]);
    if (s_editor_dock_rect_contains(&rect, cursor.x, cursor.y))
    {
      dock->drag.target = absolute_targets[i];
    }
  }

  if (dock->drag.target_leaf == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return;
  }

  LDKUIRect leaf_rect = dock->nodes[dock->drag.target_leaf].rect;
  for (u32 i = 0;
       i < sizeof(local_targets) / sizeof(local_targets[0]);
       ++i)
  {
    LDKUIRect rect =
      s_editor_dock_target_rect_local(leaf_rect, local_targets[i]);
    if (s_editor_dock_rect_contains(&rect, cursor.x, cursor.y))
    {
      dock->drag.target = local_targets[i];
    }
  }
}

static void s_editor_dock_ui_window_bring_to_front(
  LDKUIContext *ui, LDKUIId window_id)
{
  if (ui == NULL || window_id == 0 || ui->windows == NULL)
  {
    return;
  }

  u32 count = x_array_ldk_ui_window_count(ui->windows);
  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow *window = x_array_ldk_ui_window_get(ui->windows, i);
    if (window == NULL || window->id != window_id)
    {
      continue;
    }

    if (i + 1 < count)
    {
      LDKUIWindow saved = *window;
      x_array_ldk_ui_window_delete_at(ui->windows, i);
      x_array_ldk_ui_window_push(ui->windows, saved);
    }

    for (u32 z = 0; z < count; ++z)
    {
      LDKUIWindow *it = x_array_ldk_ui_window_get(ui->windows, z);
      if (it != NULL)
      {
        it->z_order = (i32)z;
      }
    }

    ui->next_z_order = (i32)count;
    return;
  }
}

static void s_editor_dock_target_overlay_disable(
  LDKEditorDockState *dock, LDKUIContext *ui)
{
  if (dock->target_overlay_window_id == 0 || ui == NULL || ui->windows == NULL)
  {
    return;
  }

  u32 count = x_array_ldk_ui_window_count(ui->windows);
  for (u32 i = 0; i < count; ++i)
  {
    LDKUIWindow *window = x_array_ldk_ui_window_get(ui->windows, i);
    if (window != NULL && window->id == dock->target_overlay_window_id)
    {
      window->rect = (LDKUIRect){0};
      window->title_bar_rect = (LDKUIRect){0};
      window->content_rect = (LDKUIRect){0};
      return;
    }
  }
}

static void s_editor_dock_targets_draw(LDKEditorDockState *dock,
  LDKEditorContext *editor, LDKUIRect workspace)
{
  static const LDKEditorDockTarget absolute_targets[] = {
    LDK_EDITOR_DOCK_TARGET_ABSOLUTE_TOP,
    LDK_EDITOR_DOCK_TARGET_ABSOLUTE_LEFT,
    LDK_EDITOR_DOCK_TARGET_ABSOLUTE_RIGHT,
    LDK_EDITOR_DOCK_TARGET_ABSOLUTE_BOTTOM
  };
  static const LDKEditorDockTarget local_targets[] = {
    LDK_EDITOR_DOCK_TARGET_LOCAL_TOP,
    LDK_EDITOR_DOCK_TARGET_LOCAL_LEFT,
    LDK_EDITOR_DOCK_TARGET_LOCAL_CENTER,
    LDK_EDITOR_DOCK_TARGET_LOCAL_RIGHT,
    LDK_EDITOR_DOCK_TARGET_LOCAL_BOTTOM
  };

  LDKUIContext *ui = &editor->ui;
  ldk_ui_begin_window_fixed(ui, "LDK Dock Targets", workspace,
    LDK_UI_WINDOW_NO_BG | LDK_UI_WINDOW_NO_PADDING);
  dock->target_overlay_window_id = ui->last_id;

  LDKUIIcon icon = {0};
  icon.color = 0xFFFFFFFF;
  icon.size = ldk_sizef(24, 24);
  icon.texture =
      ldk_renderer_texture_ui_handle(editor->renderer, editor->ui_atlas);

  for (u32 i = 0;
       i < sizeof(absolute_targets) / sizeof(absolute_targets[0]);
       ++i)
  {
    LDKEditorDockTarget target = absolute_targets[i];
    LDKUIRect rect = s_editor_dock_target_rect_absolute(workspace, target);
    LDKUIId id = (LDKUIId)(0xd0c00000u + (u32)target);
    if (target == dock->drag.target)
    {
      icon.color = LDK_EDITOR_DOCK_TARGET_COLOR_HOVER;
      icon.uv = ldk_editor_icon_rects[s_editor_dock_target_icon(target)];
    }
    else
    {
      icon.color = LDK_EDITOR_DOCK_TARGET_COLOR_IDLE;
      icon.uv = ldk_editor_icon_rects[s_editor_dock_target_icon(target)];
    }

      ldk_ui_widget_icon_label(ui, id, icon, "", rect);
  }

  if (dock->drag.target_leaf != LDK_EDITOR_DOCK_INVALID_NODE)
  {
    LDKUIRect leaf_rect = dock->nodes[dock->drag.target_leaf].rect;
    for (u32 i = 0;
         i < sizeof(local_targets) / sizeof(local_targets[0]);
         ++i)
    {
      LDKEditorDockTarget target = local_targets[i];
      LDKUIRect rect = s_editor_dock_target_rect_local(leaf_rect, target);
      LDKUIId id = (LDKUIId)(0xd0c00000u + (u32)target);
      if (target == dock->drag.target)
      {
        icon.color = LDK_EDITOR_DOCK_TARGET_COLOR_HOVER;
        icon.uv = ldk_editor_icon_rects[s_editor_dock_target_icon(target)];
      }
      else
      {
        icon.color = LDK_EDITOR_DOCK_TARGET_COLOR_IDLE;
        icon.uv = ldk_editor_icon_rects[s_editor_dock_target_icon(target)];
      }

      ldk_ui_widget_icon_label(ui, id, icon, "", rect);
    }
  }

  ldk_ui_end_window(ui);
  s_editor_dock_ui_window_bring_to_front(
    ui, dock->target_overlay_window_id);
}

static void s_editor_dock_active_drag_update(LDKEditorDockState *dock,
  LDKEditorContext *editor, LDKUIRect workspace)
{
  if (!dock->drag.active)
  {
    return;
  }

  LDKUIContext *ui = &editor->ui;
  LDKUIPoint cursor = s_editor_dock_cursor_get(ui);

  if (s_editor_dock_escape_down(ui))
  {
    s_editor_dock_target_overlay_disable(dock, ui);
    s_editor_dock_drag_reset(&dock->drag);
    return;
  }

  s_editor_dock_target_evaluate(dock, workspace, cursor);
  s_editor_dock_targets_draw(dock, editor, workspace);

  if (s_editor_dock_mouse_up(ui))
  {
    s_editor_dock_drop_commit(dock, &dock->drag, cursor);
    s_editor_dock_target_overlay_disable(dock, ui);
    s_editor_dock_drag_reset(&dock->drag);
  }
}

static bool ldk_editor_dock_init(LDKEditorContext *editor)
{
  if (editor == NULL)
  {
    return false;
  }

  LDKEditorDockState *dock = &s_editor_dock;
  *dock = (LDKEditorDockState){
    .root = LDK_EDITOR_DOCK_INVALID_NODE,
    .inspector_enabled = true
  };
  s_editor_dock_drag_reset(&dock->drag);
  s_editor_dock_resize_reset(&dock->resize);

  dock->windows[LDK_EDITOR_DOCK_WINDOW_PROJECT_EXPLORER] =
    (LDKEditorDockWindow){
      .title = "Project Explorer",
      .floating_rect = {40.0f, 90.0f, 520.0f, 400.0f},
      .leaf = LDK_EDITOR_DOCK_INVALID_NODE
    };
  dock->windows[LDK_EDITOR_DOCK_WINDOW_SCENE] =
    (LDKEditorDockWindow){
      .title = "Scene",
      .floating_rect = {180.0f, 100.0f, 640.0f, 420.0f},
      .leaf = LDK_EDITOR_DOCK_INVALID_NODE
    };
  dock->windows[LDK_EDITOR_DOCK_WINDOW_INSPECTOR] =
    (LDKEditorDockWindow){
      .title = "Inspector",
      .floating_rect = {760.0f, 100.0f, 280.0f, 420.0f},
      .leaf = LDK_EDITOR_DOCK_INVALID_NODE
    };
  dock->windows[LDK_EDITOR_DOCK_WINDOW_CONSOLE] =
    (LDKEditorDockWindow){
      .title = "Console",
      .floating_rect = {180.0f, 540.0f, 640.0f, 240.0f},
      .leaf = LDK_EDITOR_DOCK_INVALID_NODE
    };

  i32 project_leaf = s_editor_dock_leaf_create(
    dock, LDK_EDITOR_DOCK_WINDOW_PROJECT_EXPLORER);
  i32 scene_leaf = s_editor_dock_leaf_create(
    dock, LDK_EDITOR_DOCK_WINDOW_SCENE);
  const u32 bottom_windows[] = {
    LDK_EDITOR_DOCK_WINDOW_INSPECTOR,
    LDK_EDITOR_DOCK_WINDOW_CONSOLE
  };
  i32 bottom_leaf = s_editor_dock_leaf_create_tabs(dock, bottom_windows,
    sizeof(bottom_windows) / sizeof(bottom_windows[0]),
    LDK_EDITOR_DOCK_WINDOW_INSPECTOR);

  if (project_leaf == LDK_EDITOR_DOCK_INVALID_NODE ||
      scene_leaf == LDK_EDITOR_DOCK_INVALID_NODE ||
      bottom_leaf == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    *dock = (LDKEditorDockState){
      .root = LDK_EDITOR_DOCK_INVALID_NODE
    };
    return false;
  }

  i32 right_split = s_editor_dock_split_create(dock,
    LDK_EDITOR_DOCK_SPLIT_VERTICAL, 0.66f, scene_leaf, bottom_leaf);
  if (right_split == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    *dock = (LDKEditorDockState){
      .root = LDK_EDITOR_DOCK_INVALID_NODE
    };
    return false;
  }

  i32 root = s_editor_dock_split_create(dock,
    LDK_EDITOR_DOCK_SPLIT_HORIZONTAL, 0.25f, project_leaf, right_split);
  if (root == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    *dock = (LDKEditorDockState){
      .root = LDK_EDITOR_DOCK_INVALID_NODE
    };
    return false;
  }

  dock->root = root;
  dock->nodes[root].parent = LDK_EDITOR_DOCK_INVALID_NODE;
  s_editor_dock_window_locations_refresh(dock);
  dock->initialized = true;
  return true;
}

static void ldk_editor_dock_update(LDKEditorContext *editor)
{
  LDKEditorDockState *dock = &s_editor_dock;
  if (editor == NULL || !dock->initialized)
  {
    return;
  }

  LDKUIContext *ui = &editor->ui;
  LDKUIRect workspace = s_editor_dock_workspace_rect(ui);

  if (dock->root != LDK_EDITOR_DOCK_INVALID_NODE)
  {
    s_editor_dock_layout_node(dock, dock->root, workspace);
  }

  s_editor_dock_windows_draw(dock, editor);

  bool split_resizing = s_editor_dock_split_resize_update(dock, ui);
  if (!split_resizing)
  {
    s_editor_dock_tab_drag_update(dock, ui);
    s_editor_dock_floating_drag_update(dock, ui);
    s_editor_dock_active_drag_update(dock, editor, workspace);
  }
}

static void ldk_editor_dock_terminate(LDKEditorContext *editor)
{
  (void)editor;
  s_editor_dock = (LDKEditorDockState){
    .root = LDK_EDITOR_DOCK_INVALID_NODE
  };
}
