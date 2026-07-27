#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#ifndef LDK_EDITOR_DOCK_WORKSPACE_TOP
#define LDK_EDITOR_DOCK_WORKSPACE_TOP                                      \
  (LDK_UI_DEFAULT_CONTROL_HEIGHT * 2.0f + LDK_UI_DEFAULT_PADDING * 4.0f)
#endif

#ifndef LDK_EDITOR_DOCK_NODE_CAPACITY
#define LDK_EDITOR_DOCK_NODE_CAPACITY 32
#endif

#ifndef LDK_EDITOR_WINDOW_CAPACITY
#define LDK_EDITOR_WINDOW_CAPACITY 64
#endif

#ifndef LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY
#define LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY 16
#endif

#ifndef LDK_EDITOR_DOCK_LAYOUT_CAPACITY
#define LDK_EDITOR_DOCK_LAYOUT_CAPACITY 16
#endif

#ifndef LDK_EDITOR_DOCK_LAYOUT_NAME_CAPACITY
#define LDK_EDITOR_DOCK_LAYOUT_NAME_CAPACITY 64
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
#define LDK_EDITOR_DOCK_INVALID_LAYOUT UINT32_MAX
#define LDK_EDITOR_WINDOW_ID_INVALID ((LDKEditorWindowId)0)
#define LDK_EDITOR_DOCK_TML_VERSION 1

#define LDK_EDITOR_DOCK_TARGET_COLOR_IDLE 0xFFFFFFFF
#define LDK_EDITOR_DOCK_TARGET_COLOR_HOVER 0x0000FFFF

#define LDK_EDITOR_COLOR_FILE 0xFFFFFFFF
#define LDK_EDITOR_COLOR_FOLDER 0xFAD460FF

#define LDK_EDITOR_DOCK_SIZE_MAX 0.95f
#define LDK_EDITOR_DOCK_SIZE_MIN 0.05f

enum
{
  LDK_EDITOR_PROJECT_EXPLORER_INITIAL_CAPACITY = 32,
  LDK_EDITOR_PROJECT_EXPLORER_TREE_ICON_SIZE = 20,
  LDK_EDITOR_PROJECT_EXPLORER_MIN_ICON_SIZE = 20
};

/*
 * Editor window IDs are stored in the docking layout and must therefore be
 * stable across runs. The value is intentionally just an application-defined
 * integer. It must be non-zero and unique among the windows registered by the
 * editor and its tools. Do not use an address as an ID.
 */
typedef u32 LDKEditorWindowId;

typedef void (*LDKEditorWindowFunction)(LDKEditor *editor, void *data);

typedef struct LDKEditorWindow
{
  LDKEditorWindowId id;
  const char *title;
  LDKEditorWindowFunction function;
  void *data;
} LDKEditorWindow;

/*
 * Stable IDs reserved by the editor. User tools should define their own
 * persistent non-zero values outside this range.
 */
#define LDK_EDITOR_WINDOW_PROJECT_EXPLORER ((LDKEditorWindowId)0x4C444B01u)
#define LDK_EDITOR_WINDOW_SCENE            ((LDKEditorWindowId)0x4C444B02u)
#define LDK_EDITOR_WINDOW_SCENE2           ((LDKEditorWindowId)0x4C444B99u)
#define LDK_EDITOR_WINDOW_INSPECTOR        ((LDKEditorWindowId)0x4C444B03u)
#define LDK_EDITOR_WINDOW_CONSOLE          ((LDKEditorWindowId)0x4C444B04u)

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
  LDKEditorWindow window;
  LDKUIRect floating_rect;
  LDKUIId ui_window_id;
  i32 leaf;
} LDKEditorDockWindow;

typedef struct LDKEditorDockLeaf
{
  LDKEditorWindowId windows[LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY];
  u32 window_count;
  LDKEditorWindowId active_window;
  LDKUIRect tab_bar_rect;
  bool tab_bar_rect_valid;
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
  LDKEditorWindowId window;
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

typedef struct LDKEditorProjectExplorerNode
{
  XFSPath path;
  XSmallstr name;
  u32 depth;
  bool root;
} LDKEditorProjectExplorerNode;

typedef struct LDKEditorProjectExplorerEntry
{
  XFSPath path;
  XSmallstr name;
  size_t size;
  time_t last_modified;
} LDKEditorProjectExplorerEntry;

typedef struct LDKEditorProjectExplorerWindowData
{
  LDKUIPoint tree_scroll;
  LDKUIPoint file_scroll;
  XFSPath root;
  XFSPath selected_directory;
  XFSPath selected_file;
  XArray *expanded_paths;
  XArray *stack;
  XArray *dirs;
  XArray *files;
  bool root_expanded;
  float icon_size;
} LDKEditorProjectExplorerWindowData;

typedef struct LDKEditorConsoleWindowData
{
  LDKUIPoint scroll;
  XSmallstr input;
} LDKEditorConsoleWindowData;

typedef struct LDKEditorInspectorWindowData
{
  bool enabled;
} LDKEditorInspectorWindowData;

typedef struct LDKEditorDockState
{
  LDKEditorDockWindow windows[LDK_EDITOR_WINDOW_CAPACITY];
  u32 window_count;

  LDKEditorDockNode nodes[LDK_EDITOR_DOCK_NODE_CAPACITY];
  LDKEditorDockDrag drag;
  LDKEditorDockResize resize;

  LDKEditorProjectExplorerWindowData project_explorer;
  LDKEditorConsoleWindowData console;
  LDKEditorInspectorWindowData inspector;

  LDKUIId target_overlay_window_id;
  i32 root;
  bool initialized;
} LDKEditorDockState;

typedef struct LDKEditorDockLayoutWindow
{
  LDKEditorWindowId id;
  LDKUIRect floating_rect;
} LDKEditorDockLayoutWindow;

typedef struct LDKEditorDockLayoutNode
{
  LDKEditorDockNodeType type;

  union
  {
    struct
    {
      LDKEditorWindowId windows[LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY];
      u32 window_count;
      LDKEditorWindowId active_window;
    } leaf;

    struct
    {
      i32 first;
      i32 second;
      float ratio;
      LDKEditorDockSplitAxis axis;
    } split;
  } data;
} LDKEditorDockLayoutNode;

typedef struct LDKEditorDockLayout
{
  char name[LDK_EDITOR_DOCK_LAYOUT_NAME_CAPACITY];
  LDKEditorDockLayoutWindow windows[LDK_EDITOR_WINDOW_CAPACITY];
  LDKEditorDockLayoutNode nodes[LDK_EDITOR_DOCK_NODE_CAPACITY];
  u32 window_count;
  u32 node_count;
  i32 root;
} LDKEditorDockLayout;

typedef struct LDKEditorDockLayouts
{
  LDKEditorDockLayout layouts[LDK_EDITOR_DOCK_LAYOUT_CAPACITY];
  u32 layout_count;
  u32 current_layout;
} LDKEditorDockLayouts;

static LDKEditorDockState s_editor_dock;
static LDKEditorDockLayouts s_editor_dock_layouts = {
  .current_layout = LDK_EDITOR_DOCK_INVALID_LAYOUT
};

/* ------------------------------------------------------------------------- */
/* Window registry                                                           */
/* ------------------------------------------------------------------------- */

static LDKEditorDockWindow *s_editor_dock_window_get(
  LDKEditorDockState *dock, LDKEditorWindowId id)
{
  if (dock == NULL || id == LDK_EDITOR_WINDOW_ID_INVALID)
  {
    return NULL;
  }

  for (u32 i = 0; i < dock->window_count; ++i)
  {
    if (dock->windows[i].window.id == id)
    {
      return &dock->windows[i];
    }
  }

  return NULL;
}

static const LDKEditorDockWindow *s_editor_dock_window_get_const(
  const LDKEditorDockState *dock, LDKEditorWindowId id)
{
  return s_editor_dock_window_get((LDKEditorDockState *)dock, id);
}

static LDKUIRect s_editor_dock_default_floating_rect(u32 index)
{
  float offset = (float)(index % 8u) * 24.0f;
  return (LDKUIRect){
    80.0f + offset,
    100.0f + offset,
    480.0f,
    320.0f
  };
}

static bool s_editor_dock_window_add(
  LDKEditorDockState *dock, const LDKEditorWindow *window,
  LDKUIRect floating_rect)
{
  if (dock == NULL || window == NULL ||
      window->id == LDK_EDITOR_WINDOW_ID_INVALID ||
      window->title == NULL || window->title[0] == 0 ||
      window->function == NULL)
  {
    return false;
  }

  if (s_editor_dock_window_get(dock, window->id) != NULL)
  {
    return false;
  }

  if (dock->window_count >= LDK_EDITOR_WINDOW_CAPACITY)
  {
    return false;
  }

  LDKEditorDockWindow *dock_window = &dock->windows[dock->window_count++];
  *dock_window = (LDKEditorDockWindow){
    .window = *window,
    .floating_rect = floating_rect,
    .leaf = LDK_EDITOR_DOCK_INVALID_NODE
  };

  return true;
}

/*
 * Registers an editor window with the dock system.
 *
 * This may be called before or after ldk_editor_dock_init(). Windows registered
 * before initialization are preserved while the default dock tree is created.
 * Windows registered after initialization start as floating windows.
 *
 * The supplied ID must be stable across runs so that a future serialized dock
 * layout can resolve the same window at startup.
 */
bool ldk_editor_window_add(
  LDKEditor *editor, const LDKEditorWindow *window)
{
  (void)editor;

  return s_editor_dock_window_add(
    &s_editor_dock,
    window,
    s_editor_dock_default_floating_rect(s_editor_dock.window_count));
}

/* ------------------------------------------------------------------------- */
/* General helpers                                                           */
/* ------------------------------------------------------------------------- */

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
    .window = LDK_EDITOR_WINDOW_ID_INVALID,
    .source_leaf = LDK_EDITOR_DOCK_INVALID_NODE,
    .target = LDK_EDITOR_DOCK_TARGET_NONE,
    .target_leaf = LDK_EDITOR_DOCK_INVALID_NODE
  };
}

/* ------------------------------------------------------------------------- */
/* Dock tree                                                                 */
/* ------------------------------------------------------------------------- */

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
  LDKEditorDockState *dock, LDKEditorWindowId window)
{
  if (s_editor_dock_window_get(dock, window) == NULL)
  {
    return LDK_EDITOR_DOCK_INVALID_NODE;
  }

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
  LDKEditorDockState *dock, const LDKEditorWindowId *windows,
  u32 window_count, LDKEditorWindowId active_window)
{
  if (dock == NULL || windows == NULL || window_count == 0 ||
      window_count > LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY)
  {
    return LDK_EDITOR_DOCK_INVALID_NODE;
  }

  for (u32 i = 0; i < window_count; ++i)
  {
    if (s_editor_dock_window_get(dock, windows[i]) == NULL)
    {
      return LDK_EDITOR_DOCK_INVALID_NODE;
    }
  }

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
  const LDKEditorDockLeaf *leaf, LDKEditorWindowId window)
{
  if (leaf == NULL || window == LDK_EDITOR_WINDOW_ID_INVALID)
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
  LDKEditorDockLeaf *leaf, LDKEditorWindowId window)
{
  if (leaf == NULL || window == LDK_EDITOR_WINDOW_ID_INVALID)
  {
    return false;
  }

  if (s_editor_dock_leaf_contains(leaf, window))
  {
    leaf->active_window = window;
    return true;
  }

  if (leaf->window_count >= LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY)
  {
    return false;
  }

  leaf->windows[leaf->window_count++] = window;
  leaf->active_window = window;
  return true;
}

static bool s_editor_dock_leaf_remove(
  LDKEditorDockLeaf *leaf, LDKEditorWindowId window)
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
    else if (leaf->window_count == 0)
    {
      leaf->active_window = LDK_EDITOR_WINDOW_ID_INVALID;
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
  for (u32 i = 0; i < dock->window_count; ++i)
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
      LDKEditorDockWindow *window = s_editor_dock_window_get(
        dock, node->data.leaf.windows[window_index]);

      if (window != NULL)
      {
        window->leaf = node_index;
      }
    }
  }
}

static bool s_editor_dock_window_detach(
  LDKEditorDockState *dock, LDKEditorWindowId window)
{
  LDKEditorDockWindow *dock_window = s_editor_dock_window_get(dock, window);
  if (dock_window == NULL)
  {
    return false;
  }

  i32 leaf_index = dock_window->leaf;
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
  LDKEditorDockState *dock, LDKEditorWindowId window, i32 target_leaf)
{
  LDKEditorDockWindow *dock_window = s_editor_dock_window_get(dock, window);

  if (dock_window == NULL ||
      target_leaf < 0 || target_leaf >= LDK_EDITOR_DOCK_NODE_CAPACITY ||
      !dock->nodes[target_leaf].used ||
      dock->nodes[target_leaf].type != LDK_EDITOR_DOCK_NODE_LEAF)
  {
    return false;
  }

  if (dock_window->leaf == target_leaf)
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

static bool s_editor_dock_local_edge(LDKEditorDockState *dock,
  LDKEditorWindowId window, i32 target_leaf, LDKEditorDockTarget target)
{
  LDKEditorDockWindow *dock_window = s_editor_dock_window_get(dock, window);

  if (dock_window == NULL ||
      target_leaf < 0 || target_leaf >= LDK_EDITOR_DOCK_NODE_CAPACITY ||
      !dock->nodes[target_leaf].used ||
      dock->nodes[target_leaf].type != LDK_EDITOR_DOCK_NODE_LEAF)
  {
    return false;
  }

  i32 source_leaf = dock_window->leaf;
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

static bool s_editor_dock_absolute_edge(LDKEditorDockState *dock,
  LDKEditorWindowId window, LDKEditorDockTarget target)
{
  if (s_editor_dock_window_get(dock, window) == NULL ||
      !s_editor_dock_window_detach(dock, window))
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
  float ratio = new_first ? LDK_EDITOR_DOCK_SIZE_MIN : LDK_EDITOR_DOCK_SIZE_MAX;
  i32 split = s_editor_dock_split_create(dock, axis, ratio, first, second);

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
  LDKEditorWindowId window, LDKUIPoint cursor)
{
  LDKEditorDockWindow *dock_window = s_editor_dock_window_get(dock, window);
  if (dock_window == NULL)
  {
    return;
  }

  bool was_docked = dock_window->leaf != LDK_EDITOR_DOCK_INVALID_NODE;
  if (was_docked && !s_editor_dock_window_detach(dock, window))
  {
    return;
  }

  if (was_docked)
  {
    LDKUIRect *rect = &dock_window->floating_rect;
    rect->x = cursor.x - rect->w * 0.5f;
    rect->y = cursor.y - LDK_UI_TITLE_BAR_HEIGHT * 0.5f;
  }
}

static void s_editor_dock_drop_commit(LDKEditorDockState *dock,
  LDKEditorDockDrag *drag, LDKUIPoint cursor)
{
  if (dock == NULL || drag == NULL ||
      s_editor_dock_window_get(dock, drag->window) == NULL)
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

/* ------------------------------------------------------------------------- */
/* Layout and splitter interaction                                           */
/* ------------------------------------------------------------------------- */

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

  float ratio = s_editor_dock_clampf(node->data.split.ratio, LDK_EDITOR_DOCK_SIZE_MIN, LDK_EDITOR_DOCK_SIZE_MAX);
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
          s_editor_dock_clampf(position / size, LDK_EDITOR_DOCK_SIZE_MIN, LDK_EDITOR_DOCK_SIZE_MAX);
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

/* ------------------------------------------------------------------------- */
/* Built-in editor windows                                                   */
/* ------------------------------------------------------------------------- */


static bool s_editor_project_explorer_initialize(
  LDKEditorProjectExplorerWindowData *state)
{
  if (state->expanded_paths == NULL)
  {
    state->expanded_paths = x_array_create(
      sizeof(XFSPath), LDK_EDITOR_PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  if (state->stack == NULL)
  {
    state->stack = x_array_create(
      sizeof(LDKEditorProjectExplorerNode),
      LDK_EDITOR_PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  if (state->dirs == NULL)
  {
    state->dirs = x_array_create(
      sizeof(LDKEditorProjectExplorerEntry),
      LDK_EDITOR_PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  if (state->files == NULL)
  {
    state->files = x_array_create(
      sizeof(LDKEditorProjectExplorerEntry),
      LDK_EDITOR_PROJECT_EXPLORER_INITIAL_CAPACITY);
  }

  return state->expanded_paths != NULL &&
         state->stack != NULL &&
         state->dirs != NULL &&
         state->files != NULL;
}

static i32 s_editor_project_explorer_expanded_path_index(
  LDKEditorProjectExplorerWindowData *state, const XFSPath *path)
{
  for (u32 i = 0; i < x_array_count(state->expanded_paths); ++i)
  {
    XFSPath *expanded_path = x_array_get(state->expanded_paths, i);
    if (x_fs_path_compare(expanded_path, path) == 0)
    {
      return (i32)i;
    }
  }

  return -1;
}

static bool s_editor_project_explorer_root_set(
  LDKEditorProjectExplorerWindowData *state, const char *root_path)
{
  if (root_path == NULL)
  {
    return false;
  }

  XFSPath root = {0};
  x_fs_path_set(&root, root_path);
  x_fs_path_normalize(&root);

  if (!x_fs_path_is_directory(&root))
  {
    return false;
  }

  if (state->root.length == 0 ||
      x_fs_path_compare(&state->root, &root) != 0)
  {
    state->root = root;
    state->selected_directory = root;
    memset(&state->selected_file, 0, sizeof(state->selected_file));
    x_array_clear(state->expanded_paths);
    state->root_expanded = true;
  }

  return true;
}

static void s_editor_project_explorer_directory_select(
  LDKEditorProjectExplorerWindowData *state,
  const XFSPath *path, bool expand)
{
  state->selected_directory = *path;
  memset(&state->selected_file, 0, sizeof(state->selected_file));

  if (!expand)
  {
    return;
  }

  if (x_fs_path_compare(&state->root, path) == 0)
  {
    state->root_expanded = true;
    return;
  }

  if (s_editor_project_explorer_expanded_path_index(state, path) < 0)
  {
    x_array_add(state->expanded_paths, (XFSPath *)path);
  }
}

static void s_editor_project_explorer_entry_insert_sorted(
  XArray *entries, const LDKEditorProjectExplorerEntry *entry)
{
  u32 insert_index = x_array_count(entries);

  for (u32 i = 0; i < x_array_count(entries); ++i)
  {
    LDKEditorProjectExplorerEntry *it = x_array_get(entries, i);
    if (strcmp(it->name.buf, entry->name.buf) > 0)
    {
      insert_index = i;
      break;
    }
  }

  x_array_insert(entries, (void *)entry, insert_index);
}

static void s_editor_project_explorer_directory_read(
  const XFSPath *path, XArray *dirs, XArray *files)
{
  if (dirs != NULL)
  {
    x_array_clear(dirs);
  }

  if (files != NULL)
  {
    x_array_clear(files);
  }

  XFSDireEntry fs_entry = {0};
  XFSDireHandle *dir =
    x_fs_find_first_file(x_fs_path_cstr(path), &fs_entry);

  while (dir != NULL)
  {
    bool special_entry = strcmp(fs_entry.name, ".") == 0 ||
                         strcmp(fs_entry.name, "..") == 0;
    XArray *target = fs_entry.is_directory ? dirs : files;

    if (!special_entry && target != NULL)
    {
      LDKEditorProjectExplorerEntry entry = {0};
      entry.path = *path;
      x_fs_path_join(&entry.path, fs_entry.name);
      x_smallstr_from_cstr(&entry.name, fs_entry.name);
      entry.size = fs_entry.size;
      entry.last_modified = fs_entry.last_modified;
      s_editor_project_explorer_entry_insert_sorted(target, &entry);
    }

    if (!x_fs_find_next_file(dir, &fs_entry))
    {
      break;
    }
  }

  if (dir != NULL)
  {
    x_fs_find_close(dir);
  }
}

static bool s_editor_project_explorer_tree_node(
  LDKEditorProjectExplorerWindowData *state,
  LDKUIContext *ui,
  const LDKEditorProjectExplorerNode *node,
  LDKUIIcon folder_icon)
{
  i32 expanded_index = node->root
    ? -1
    : s_editor_project_explorer_expanded_path_index(state, &node->path);
  bool was_expanded = node->root
    ? state->root_expanded
    : expanded_index >= 0;
  u32 flags = 0;

  if (x_fs_path_compare(&state->selected_directory, &node->path) == 0)
  {
    flags |= LDK_UI_TREE_NODE_SELECTED;
  }

  u32 result = ldk_ui_tree_node_ex(
    ui, node->name.buf, folder_icon, was_expanded, node->depth, flags);
  bool expanded = was_expanded;

  if (result & LDK_UI_TREE_NODE_RESULT_CLICKED)
  {
    s_editor_project_explorer_directory_select(
      state, &node->path, false);
  }

  if (result & LDK_UI_TREE_NODE_RESULT_TOGGLED)
  {
    expanded = !was_expanded;
  }

  if (expanded == was_expanded)
  {
    return expanded;
  }

  s_editor_project_explorer_directory_select(
    state, &node->path, false);

  if (node->root)
  {
    state->root_expanded = expanded;
  }
  else if (expanded)
  {
    x_array_add(state->expanded_paths, (XFSPath *)&node->path);
  }
  else
  {
    x_array_delete_at(state->expanded_paths, (u32)expanded_index);
  }

  return expanded;
}

static void s_editor_project_explorer_tree_draw(
  LDKEditorProjectExplorerWindowData *state,
  LDKUIContext *ui,
  LDKUIIcon folder_icon)
{
  ldk_ui_set_next_width(ui, ldk_ui_px(220.0f));
  state->tree_scroll = ldk_ui_begin_scrollview(
    ui, state->tree_scroll,
    LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  x_array_clear(state->stack);

  LDKEditorProjectExplorerNode root_node = {0};
  root_node.path = state->root;
  x_fs_path_basename(&state->root, &root_node.name);

  if (root_node.name.length == 0)
  {
    x_smallstr_from_cstr(
      &root_node.name, x_fs_path_cstr(&state->root));
  }

  root_node.root = true;
  x_array_add(state->stack, &root_node);

  while (x_array_count(state->stack) > 0)
  {
    u32 stack_index = x_array_count(state->stack) - 1;
    LDKEditorProjectExplorerNode *node_ptr =
      x_array_get(state->stack, stack_index);
    LDKEditorProjectExplorerNode node = *node_ptr;
    x_array_delete_at(state->stack, stack_index);

    if (!s_editor_project_explorer_tree_node(
          state, ui, &node, folder_icon))
    {
      continue;
    }

    s_editor_project_explorer_directory_read(
      &node.path, state->dirs, NULL);

    for (u32 i = x_array_count(state->dirs); i > 0; --i)
    {
      LDKEditorProjectExplorerEntry *entry =
        x_array_get(state->dirs, i - 1);
      LDKEditorProjectExplorerNode child = {0};
      child.path = entry->path;
      child.name = entry->name;
      child.depth = node.depth + 1;
      x_array_add(state->stack, &child);
    }
  }

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
}

static LDKEditorProjectExplorerEntry *
s_editor_project_explorer_entry_get(
  LDKEditorProjectExplorerWindowData *state,
  u32 index, bool *is_directory)
{
  u32 directory_count = x_array_count(state->dirs);
  *is_directory = index < directory_count;

  if (*is_directory)
  {
    return x_array_get(state->dirs, index);
  }

  return x_array_get(state->files, index - directory_count);
}

static void s_editor_project_explorer_entry_activate(
  LDKEditorProjectExplorerWindowData *state,
  const LDKEditorProjectExplorerEntry *entry,
  bool is_directory)
{
  if (is_directory)
  {
    s_editor_project_explorer_directory_select(
      state, &entry->path, true);
  }
  else
  {
    state->selected_file = entry->path;
  }
}

static void s_editor_project_explorer_entries_draw(
  LDKEditorProjectExplorerWindowData *state,
  LDKUIContext *ui,
  LDKUIIcon folder_icon,
  LDKUIIcon file_icon)
{
  u32 total_count =
    x_array_count(state->dirs) + x_array_count(state->files);
  bool compact_mode =
    state->icon_size <= LDK_EDITOR_PROJECT_EXPLORER_MIN_ICON_SIZE;

  if (compact_mode)
  {
    for (u32 entry_index = 0;
         entry_index < total_count;
         ++entry_index)
    {
      bool is_directory = false;
      LDKEditorProjectExplorerEntry *entry =
        s_editor_project_explorer_entry_get(
          state, entry_index, &is_directory);

      ldk_ui_set_next_height(
        ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
      ldk_ui_begin_horizontal(ui);

      ldk_ui_set_next_weight(ui, 0.0f);
      bool icon_clicked = ldk_ui_icon_button(
        ui, is_directory ? folder_icon : file_icon, NULL);
      bool label_clicked =
        ldk_ui_button_flat(ui, entry->name.buf);

      ldk_ui_end_horizontal(ui);

      if (icon_clicked || label_clicked)
      {
        s_editor_project_explorer_entry_activate(
          state, entry, is_directory);
      }
    }

    return;
  }

  float tile_w = state->icon_size + 32.0f;
  float tile_h =
    state->icon_size + LDK_UI_DEFAULT_CONTROL_HEIGHT + 12.0f;
  float available_w = ui->current_layout != NULL
    ? ui->current_layout->content_rect.w
    : tile_w;
  u32 column_count = (u32)(available_w / tile_w);

  if (column_count == 0)
  {
    column_count = 1;
  }

  u32 tile_index = 0;
  while (tile_index < total_count)
  {
    ldk_ui_set_next_height(ui, ldk_ui_px(tile_h));
    ldk_ui_begin_horizontal(ui);

    for (u32 column = 0;
         column < column_count && tile_index < total_count;
         ++column, ++tile_index)
    {
      bool is_directory = false;
      LDKEditorProjectExplorerEntry *entry =
        s_editor_project_explorer_entry_get(
          state, tile_index, &is_directory);

      ldk_ui_set_next_size(
        ui, ldk_ui_px(tile_w), ldk_ui_px(tile_h));
      ldk_ui_begin_vertical(ui);

      ldk_ui_set_next_size(
        ui,
        ldk_ui_px(state->icon_size),
        ldk_ui_px(state->icon_size));
      bool icon_clicked = ldk_ui_icon_button(
        ui, is_directory ? folder_icon : file_icon, NULL);

      ldk_ui_set_next_height(
        ui, ldk_ui_px(LDK_UI_DEFAULT_CONTROL_HEIGHT));
      bool label_clicked =
        ldk_ui_button_flat(ui, entry->name.buf);

      ldk_ui_end_vertical(ui);

      if (icon_clicked || label_clicked)
      {
        s_editor_project_explorer_entry_activate(
          state, entry, is_directory);
      }
    }

    ldk_ui_spacer(ui);
    ldk_ui_end_horizontal(ui);
  }
}

static void s_editor_project_explorer_files_draw(
  LDKEditorProjectExplorerWindowData *state,
  LDKUIContext *ui,
  LDKUIIcon folder_icon,
  LDKUIIcon file_icon)
{
  ldk_ui_begin_vertical(ui);
  ldk_ui_set_next_weight(ui, 0.0f);

  XFSPath relative_directory = {0};
  if (x_fs_path_relative_to(
        &state->root,
        &state->selected_directory,
        &relative_directory) > 0)
  {
    ldk_ui_label(ui, relative_directory.buf);
  }
  else
  {
    ldk_ui_label(ui, state->selected_directory.buf);
  }

  ldk_ui_set_next_weight(ui, 0.0f);
  state->icon_size = ldk_ui_slider(
    ui,
    state->icon_size,
    LDK_EDITOR_PROJECT_EXPLORER_MIN_ICON_SIZE,
    72.0f);

  folder_icon.size =
    ldk_sizef(state->icon_size, state->icon_size);
  file_icon.size = folder_icon.size;

  state->file_scroll = ldk_ui_begin_scrollview(
    ui, state->file_scroll,
    LDK_UI_SCROLL_VERTICAL | LDK_UI_SCROLL_IF_NEEDED);

  s_editor_project_explorer_directory_read(
    &state->selected_directory, state->dirs, state->files);
  s_editor_project_explorer_entries_draw(
    state, ui, folder_icon, file_icon);

  ldk_ui_spacer(ui);
  ldk_ui_end_scrollview(ui);
  ldk_ui_end_vertical(ui);
}

static void s_editor_project_explorer_window(
  LDKEditor *opaque_editor, void *data)
{
  LDKEditorContext *editor = (LDKEditorContext *)opaque_editor;
  LDKEditorProjectExplorerWindowData *state =
    (LDKEditorProjectExplorerWindowData *)data;
  LDKUIContext *ui = &editor->ui;
  const XFSPath *root = NULL;

  if (editor->project.loaded &&
      editor->project.project_root_path.length > 0)
  {
    root = &editor->project.project_root_path;
  }
  else if (editor->engine_runtree.length > 0)
  {
    root = &editor->engine_runtree;
  }

  if (!s_editor_project_explorer_initialize(state))
  {
    ldk_ui_label(ui, "Project explorer allocation failed.");
    return;
  }

  if (root == NULL ||
      !s_editor_project_explorer_root_set(state, root->buf))
  {
    ldk_ui_label(ui, "No project root.");
    return;
  }

  LDKUIIcon file_icon = {0};
  file_icon.size =
    ldk_sizef(state->icon_size, state->icon_size);
  file_icon.texture =
    ldk_renderer_texture_ui_handle(
      editor->renderer, editor->ui_atlas);
  file_icon.uv =
    ldk_editor_icon_rects[LDK_EDITOR_ICON_FILE];
  file_icon.color = LDK_EDITOR_COLOR_FILE;

  LDKUIIcon folder_icon = file_icon;
  folder_icon.uv =
    ldk_editor_icon_rects[LDK_EDITOR_ICON_FOLDER];
  folder_icon.color = LDK_EDITOR_COLOR_FOLDER;

  LDKUIIcon tree_folder_icon = folder_icon;
  tree_folder_icon.size = ldk_sizef(
    LDK_EDITOR_PROJECT_EXPLORER_TREE_ICON_SIZE,
    LDK_EDITOR_PROJECT_EXPLORER_TREE_ICON_SIZE);

  ldk_ui_begin_horizontal(ui);
  s_editor_project_explorer_tree_draw(
    state, ui, tree_folder_icon);
  s_editor_project_explorer_files_draw(
    state, ui, folder_icon, file_icon);
  ldk_ui_end_horizontal(ui);
}

static void s_editor_scene_window(LDKEditor *opaque_editor, void *data)
{
  LDKEditorContext *editor = (LDKEditorContext *)opaque_editor;
  LDKUIContext *ui = &editor->ui;
  (void)data;

  ldk_ui_label(ui, "Scene viewport placeholder");
  ldk_ui_horizontal_line(ui);
  ldk_ui_button(ui, "Frame selected");
  ldk_ui_button(ui, "Toggle grid");
  ldk_ui_spacer(ui);
}

static void s_editor_inspector_window(
  LDKEditor *opaque_editor, void *data)
{
  LDKEditorContext *editor = (LDKEditorContext *)opaque_editor;
  LDKEditorInspectorWindowData *state =
    (LDKEditorInspectorWindowData *)data;
  LDKUIContext *ui = &editor->ui;

  ldk_ui_label(ui, "Inspector placeholder");
  ldk_ui_horizontal_line(ui);
  ldk_ui_label(ui, "Name: Selected Entity");
  state->enabled = ldk_ui_toggle(ui, state->enabled);
  ldk_ui_label(ui, state->enabled ? "Enabled" : "Disabled");
  ldk_ui_button(ui, "Add Component");
  ldk_ui_spacer(ui);
}

static void s_editor_console_window(
  LDKEditor *opaque_editor, void *data)
{
  LDKEditorContext *editor = (LDKEditorContext *)opaque_editor;
  LDKEditorConsoleWindowData *state =
    (LDKEditorConsoleWindowData *)data;
  LDKUIContext *ui = &editor->ui;

  state->scroll = ldk_ui_begin_scrollview(ui, state->scroll,
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

  ldk_ui_set_next_weight(ui, 0.0f);
  if (ldk_ui_input_box(
        ui, state->input.buf, X_SMALLSTR_MAX_LENGTH) &
      LDK_UI_INPUT_BOX_COMMITTED)
  {
    if (editor->console_sb != NULL)
    {
      x_strbuilder_append_format(
        editor->console_sb, "%s\n", state->input.buf);
    }

    ldk_editor_command_run(opaque_editor, state->input.buf);
    x_smallstr_clear(&state->input);
    state->scroll.y += 10000.0f;
  }
}

static bool s_editor_builtin_windows_add(
  LDKEditorContext *editor, LDKEditorDockState *dock)
{
  if (s_editor_dock_window_get(
        dock, LDK_EDITOR_WINDOW_PROJECT_EXPLORER) == NULL)
  {
    LDKEditorWindow window = {
      .id = LDK_EDITOR_WINDOW_PROJECT_EXPLORER,
      .title = "Project Explorer",
      .function = s_editor_project_explorer_window,
      .data = &dock->project_explorer
    };

    if (!s_editor_dock_window_add(
          dock, &window, (LDKUIRect){40.0f, 90.0f, 520.0f, 400.0f}))
    {
      return false;
    }
  }

  if (s_editor_dock_window_get(dock, LDK_EDITOR_WINDOW_SCENE) == NULL)
  {
    LDKEditorWindow window = {
      .id = LDK_EDITOR_WINDOW_SCENE,
      .title = "Scene",
      .function = s_editor_scene_window,
      .data = NULL
    };

    if (!s_editor_dock_window_add(
          dock, &window, (LDKUIRect){180.0f, 100.0f, 640.0f, 420.0f}))
    {
      return false;
    }
  }

  if (s_editor_dock_window_get(dock, LDK_EDITOR_WINDOW_INSPECTOR) == NULL)
  {
    LDKEditorWindow window = {
      .id = LDK_EDITOR_WINDOW_INSPECTOR,
      .title = "Inspector",
      .function = s_editor_inspector_window,
      .data = &dock->inspector
    };

    if (!s_editor_dock_window_add(
          dock, &window, (LDKUIRect){760.0f, 100.0f, 280.0f, 420.0f}))
    {
      return false;
    }
  }

  if (s_editor_dock_window_get(dock, LDK_EDITOR_WINDOW_CONSOLE) == NULL)
  {
    LDKEditorWindow window = {
      .id = LDK_EDITOR_WINDOW_CONSOLE,
      .title = "Console",
      .function = s_editor_console_window,
      .data = &dock->console
    };

    if (!s_editor_dock_window_add(
          dock, &window, (LDKUIRect){180.0f, 540.0f, 640.0f, 240.0f}))
    {
      return false;
    }
  }

  (void)editor;
  return true;
}

/* ------------------------------------------------------------------------- */
/* Window drawing                                                            */
/* ------------------------------------------------------------------------- */

static void s_editor_dock_window_content_draw(
  LDKEditorDockState *dock, LDKEditorContext *editor,
  LDKEditorWindowId window_id)
{
  LDKEditorDockWindow *window =
    s_editor_dock_window_get(dock, window_id);

  if (window != NULL && window->window.function != NULL)
  {
    window->window.function((LDKEditor *)editor, window->window.data);
  }
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
  char window_title[32];
  snprintf(window_title, sizeof(window_title), "Dock Leaf %d", leaf_index);

  ldk_ui_begin_window_fixed(ui, window_title, node->rect,
    LDK_UI_WINDOW_BORDER | LDK_UI_WINDOW_NO_PADDING);
  LDKUIId dock_window_id = ui->last_id;

  LDKUITabBarItem tab_items[LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY] = {0};
  u32 active_index = 0;

  for (u32 i = 0; i < leaf->window_count; ++i)
  {
    const LDKEditorDockWindow *window =
      s_editor_dock_window_get_const(dock, leaf->windows[i]);

    tab_items[i] = (LDKUITabBarItem){
      .id = (LDKUIId)(i + 1),
      .label = window != NULL ? window->window.title : "<missing window>"
    };

    if (leaf->windows[i] == leaf->active_window)
    {
      active_index = i;
    }
  }

  LDKUITabBarResult tab_result =
    ldk_ui_tab_bar(ui, tab_items, leaf->window_count, active_index);
  leaf->tab_bar_rect = ldk_ui_last_rect(ui);
  leaf->tab_bar_rect_valid = true;

  if (tab_result.active_index < leaf->window_count)
  {
    leaf->active_window = leaf->windows[tab_result.active_index];
  }

  LDKEditorDockWindow *active_window =
    s_editor_dock_window_get(dock, leaf->active_window);

  if (active_window != NULL)
  {
    active_window->ui_window_id = dock_window_id;
    s_editor_dock_window_content_draw(
      dock, editor, leaf->active_window);
  }

  ldk_ui_end_window(ui);
}

static void s_editor_dock_floating_window_draw(
  LDKEditorDockState *dock, LDKEditorContext *editor,
  LDKEditorDockWindow *window)
{
  LDKUIContext *ui = &editor->ui;
  u32 flags = LDK_UI_WINDOW_TITLE_BAR |
              LDK_UI_WINDOW_DRAGGABLE |
              LDK_UI_WINDOW_RESIZABLE |
              LDK_UI_WINDOW_BORDER;

  window->floating_rect = ldk_ui_begin_window(
    ui, window->window.title, window->floating_rect, flags);
  window->ui_window_id = ui->last_id;
  s_editor_dock_window_content_draw(dock, editor, window->window.id);
  ldk_ui_end_window(ui);
}

static void s_editor_dock_windows_draw(
  LDKEditorDockState *dock, LDKEditorContext *editor)
{
  for (i32 node_index = 0;
       node_index < LDK_EDITOR_DOCK_NODE_CAPACITY;
       ++node_index)
  {
    LDKEditorDockNode *node = &dock->nodes[node_index];

    if (node->used && node->type == LDK_EDITOR_DOCK_NODE_LEAF)
    {
      node->data.leaf.tab_bar_rect_valid = false;
    }
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

  for (u32 i = 0; i < dock->window_count; ++i)
  {
    LDKEditorDockWindow *window = &dock->windows[i];
    if (window->leaf == LDK_EDITOR_DOCK_INVALID_NODE)
    {
      s_editor_dock_floating_window_draw(dock, editor, window);
    }
  }
}

/* ------------------------------------------------------------------------- */
/* Dragging                                                                  */
/* ------------------------------------------------------------------------- */

static void s_editor_dock_tab_drag_update(
  LDKEditorDockState *dock, LDKUIContext *ui)
{
  LDKUIPoint cursor = s_editor_dock_cursor_get(ui);

  if (!dock->drag.pending && !dock->drag.active &&
      s_editor_dock_mouse_down(ui))
  {
    for (i32 leaf_index = 0;
         leaf_index < LDK_EDITOR_DOCK_NODE_CAPACITY;
         ++leaf_index)
    {
      LDKEditorDockNode *node = &dock->nodes[leaf_index];

      if (!node->used || node->type != LDK_EDITOR_DOCK_NODE_LEAF)
      {
        continue;
      }

      LDKEditorDockLeaf *leaf = &node->data.leaf;

      if (!leaf->tab_bar_rect_valid ||
          !s_editor_dock_rect_contains(
            &leaf->tab_bar_rect, cursor.x, cursor.y))
      {
        continue;
      }

      dock->drag.pending = true;
      dock->drag.window = leaf->active_window;
      dock->drag.source_leaf = leaf_index;
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
    /*
     * A release without crossing the drag threshold is a normal tab click.
     * ldk_ui_tab_bar() has already selected the clicked tab earlier in this
     * frame. Only clear the pending drag; never restore drag.window here.
     */
    s_editor_dock_drag_reset(&dock->drag);
  }
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

  for (u32 i = 0; i < dock->window_count; ++i)
  {
    LDKEditorDockWindow *window = &dock->windows[i];

    if (window->leaf == LDK_EDITOR_DOCK_INVALID_NODE &&
        window->ui_window_id == ui->dragging_window_id)
    {
      dock->drag.window = window->window.id;
      dock->drag.source_leaf = LDK_EDITOR_DOCK_INVALID_NODE;
      dock->drag.active = true;
      return;
    }
  }
}

/* ------------------------------------------------------------------------- */
/* Dock targets                                                              */
/* ------------------------------------------------------------------------- */

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

static LDKEditorIcon s_editor_dock_target_icon(
  LDKEditorDockTarget target)
{
  switch (target)
  {
    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_TOP:
      return LDK_EDITOR_ICON_DOCK_TOP;
    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_LEFT:
      return LDK_EDITOR_ICON_DOCK_LEFT;
    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_RIGHT:
      return LDK_EDITOR_ICON_DOCK_RIGHT;
    case LDK_EDITOR_DOCK_TARGET_ABSOLUTE_BOTTOM:
      return LDK_EDITOR_ICON_DOCK_BOTTOM;
    case LDK_EDITOR_DOCK_TARGET_LOCAL_TOP:
      return LDK_EDITOR_ICON_DOCK_TOP;
    case LDK_EDITOR_DOCK_TARGET_LOCAL_LEFT:
      return LDK_EDITOR_ICON_DOCK_LEFT;
    case LDK_EDITOR_DOCK_TARGET_LOCAL_CENTER:
      return LDK_EDITOR_ICON_DOCK_CENTER;
    case LDK_EDITOR_DOCK_TARGET_LOCAL_RIGHT:
      return LDK_EDITOR_ICON_DOCK_RIGHT;
    case LDK_EDITOR_DOCK_TARGET_LOCAL_BOTTOM:
      return LDK_EDITOR_ICON_DOCK_BOTTOM;
    default:
      return -1;
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
  if (dock->target_overlay_window_id == 0 ||
      ui == NULL || ui->windows == NULL)
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

    icon.color = target == dock->drag.target
                   ? LDK_EDITOR_DOCK_TARGET_COLOR_HOVER
                   : LDK_EDITOR_DOCK_TARGET_COLOR_IDLE;
    icon.uv = ldk_editor_icon_rects[s_editor_dock_target_icon(target)];

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

      icon.color = target == dock->drag.target
                     ? LDK_EDITOR_DOCK_TARGET_COLOR_HOVER
                     : LDK_EDITOR_DOCK_TARGET_COLOR_IDLE;
      icon.uv = ldk_editor_icon_rects[s_editor_dock_target_icon(target)];

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

/* ------------------------------------------------------------------------- */
/* Serialization                                                             */
/* ------------------------------------------------------------------------- */

typedef struct LDKEditorDockLayoutSnapshotContext
{
  const LDKEditorDockState *dock;
  LDKEditorDockLayout *layout;
  LDKEditorWindowId docked_windows[LDK_EDITOR_WINDOW_CAPACITY];
  bool visited_nodes[LDK_EDITOR_DOCK_NODE_CAPACITY];
  u32 docked_window_count;
} LDKEditorDockLayoutSnapshotContext;

typedef struct LDKEditorDockLayoutReadContext
{
  const TMLDocument *document;
  const LDKEditorDockState *dock;
  LDKEditorDockLayout *layout;
  LDKEditorWindowId saved_windows[LDK_EDITOR_WINDOW_CAPACITY];
  LDKEditorWindowId docked_windows[LDK_EDITOR_WINDOW_CAPACITY];
  u32 saved_window_count;
  u32 docked_window_count;
  u32 source_node_count;
} LDKEditorDockLayoutReadContext;

static bool s_editor_dock_id_list_contains(
    const LDKEditorWindowId *ids, u32 count, LDKEditorWindowId id)
{
  for (u32 i = 0; i < count; ++i)
  {
    if (ids[i] == id)
    {
      return true;
    }
  }

  return false;
}

static bool s_editor_dock_id_list_add(
    LDKEditorWindowId *ids, u32 *count, u32 capacity, LDKEditorWindowId id)
{
  if (ids == NULL || count == NULL || *count >= capacity ||
      id == LDK_EDITOR_WINDOW_ID_INVALID ||
      s_editor_dock_id_list_contains(ids, *count, id))
  {
    return false;
  }

  ids[(*count)++] = id;
  return true;
}

static bool s_editor_dock_layout_name_copy(
    char *destination, size_t destination_size, TMLString name)
{
  if (destination == NULL || destination_size == 0 || name.data == NULL ||
      name.size == 0 || name.size >= destination_size)
  {
    return false;
  }

  memcpy(destination, name.data, name.size);
  destination[name.size] = 0;
  return true;
}

static bool s_editor_dock_tml_string_equals(TMLString string, const char *text)
{
  if (string.data == NULL || text == NULL)
  {
    return false;
  }

  size_t text_length = strlen(text);
  return string.size == text_length &&
         memcmp(string.data, text, text_length) == 0;
}

static bool s_editor_dock_float_from_entry(const TMLEntry *entry, float *value)
{
  if (entry == NULL || value == NULL)
  {
    return false;
  }

  double number;
  if (entry->type == TML_VALUE_F64)
  {
    f64 parsed_number;
    if (!tml_entry_get_f64(entry, &parsed_number))
    {
      return false;
    }
    number = parsed_number;
  }
  else if (entry->type == TML_VALUE_I64)
  {
    i64 parsed_integer;
    if (!tml_entry_get_i64(entry, &parsed_integer))
    {
      return false;
    }
    number = (double)parsed_integer;
  }
  else
  {
    return false;
  }

  if (!isfinite(number) || number < -FLT_MAX || number > FLT_MAX)
  {
    return false;
  }

  *value = (float)number;
  return true;
}

static bool s_editor_dock_window_id_from_entry(
    const TMLEntry *entry, LDKEditorWindowId *id)
{
  i64 value;

  if (entry == NULL || id == NULL || !tml_entry_get_i64(entry, &value) ||
      value <= 0 || value > UINT32_MAX)
  {
    return false;
  }

  *id = (LDKEditorWindowId)value;
  return true;
}

static bool s_editor_dock_rect_from_entry(
    const TMLDocument *document, const TMLEntry *entry, LDKUIRect *rect)
{
  double values[4];

  if (document == NULL || entry == NULL || rect == NULL)
  {
    return false;
  }

  if (entry->type == TML_VALUE_ARRAY_F64)
  {
    TMLF64Slice array;
    if (!tml_entry_get_f64_array(document, entry, &array) || array.count != 4)
    {
      return false;
    }

    for (u32 i = 0; i < 4; ++i)
    {
      values[i] = array.data[i];
    }
  }
  else if (entry->type == TML_VALUE_ARRAY_I64)
  {
    TMLI64Slice array;
    if (!tml_entry_get_i64_array(document, entry, &array) || array.count != 4)
    {
      return false;
    }

    for (u32 i = 0; i < 4; ++i)
    {
      values[i] = (double)array.data[i];
    }
  }
  else
  {
    return false;
  }

  for (u32 i = 0; i < 4; ++i)
  {
    if (!isfinite(values[i]) || values[i] < -FLT_MAX || values[i] > FLT_MAX)
    {
      return false;
    }
  }

  if (values[2] <= 0.0 || values[3] <= 0.0)
  {
    return false;
  }

  *rect = (LDKUIRect){
      (float)values[0], (float)values[1], (float)values[2], (float)values[3]};
  return true;
}

static void s_editor_dock_tml_indent(XStrBuilder *out, u32 indent)
{
  for (u32 i = 0; i < indent; ++i)
  {
    x_strbuilder_append(out, "  ");
  }
}

static void s_editor_dock_tml_string_append(XStrBuilder *out, const char *text)
{
  x_strbuilder_append_char(out, '"');

  if (text != NULL)
  {
    for (const char *character = text; *character != 0; ++character)
    {
      switch (*character)
      {
      case '\n':
        x_strbuilder_append(out, "\\n");
        break;

      case '\r':
        x_strbuilder_append(out, "\\r");
        break;

      case '\t':
        x_strbuilder_append(out, "\\t");
        break;

      case '"':
        x_strbuilder_append(out, "\\\"");
        break;

      case '\\':
        x_strbuilder_append(out, "\\\\");
        break;

      default:
        x_strbuilder_append_char(out, *character);
        break;
      }
    }
  }

  x_strbuilder_append_char(out, '"');
}

static bool s_editor_dock_layout_snapshot_node(
    LDKEditorDockLayoutSnapshotContext *context, i32 source_node,
    i32 *layout_node)
{
  if (context == NULL || layout_node == NULL || source_node < 0 ||
      source_node >= LDK_EDITOR_DOCK_NODE_CAPACITY ||
      context->visited_nodes[source_node] ||
      context->layout->node_count >= LDK_EDITOR_DOCK_NODE_CAPACITY)
  {
    return false;
  }

  const LDKEditorDockNode *source = &context->dock->nodes[source_node];
  if (!source->used)
  {
    return false;
  }

  context->visited_nodes[source_node] = true;

  i32 destination_index = (i32)context->layout->node_count++;
  LDKEditorDockLayoutNode *destination =
      &context->layout->nodes[destination_index];
  destination->type = source->type;

  if (source->type == LDK_EDITOR_DOCK_NODE_LEAF)
  {
    const LDKEditorDockLeaf *leaf = &source->data.leaf;
    if (leaf->window_count == 0 ||
        leaf->window_count > LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY)
    {
      return false;
    }

    for (u32 i = 0; i < leaf->window_count; ++i)
    {
      LDKEditorWindowId window = leaf->windows[i];
      if (s_editor_dock_window_get_const(context->dock, window) == NULL ||
          !s_editor_dock_id_list_add(context->docked_windows,
              &context->docked_window_count, LDK_EDITOR_WINDOW_CAPACITY,
              window))
      {
        return false;
      }

      destination->data.leaf.windows[i] = window;
    }

    destination->data.leaf.window_count = leaf->window_count;
    destination->data.leaf.active_window =
        s_editor_dock_leaf_contains(leaf, leaf->active_window)
            ? leaf->active_window
            : leaf->windows[0];
  }
  else if (source->type == LDK_EDITOR_DOCK_NODE_SPLIT)
  {
    const LDKEditorDockSplit *split = &source->data.split;
    if ((split->axis != LDK_EDITOR_DOCK_SPLIT_HORIZONTAL &&
            split->axis != LDK_EDITOR_DOCK_SPLIT_VERTICAL) ||
        !isfinite(split->ratio))
    {
      return false;
    }

    i32 first;
    i32 second;
    if (!s_editor_dock_layout_snapshot_node(context, split->first, &first) ||
        !s_editor_dock_layout_snapshot_node(context, split->second, &second))
    {
      return false;
    }

    destination = &context->layout->nodes[destination_index];
    destination->data.split.first = first;
    destination->data.split.second = second;
    destination->data.split.ratio = s_editor_dock_clampf(
        split->ratio, LDK_EDITOR_DOCK_SIZE_MIN, LDK_EDITOR_DOCK_SIZE_MAX);
    destination->data.split.axis = split->axis;
  }
  else
  {
    return false;
  }

  *layout_node = destination_index;
  return true;
}

static bool s_editor_dock_layout_snapshot(LDKEditorDockLayout *layout,
    const LDKEditorDockState *dock, const char *name)
{
  LDKEditorDockLayoutSnapshotContext context;
  size_t name_length;

  if (layout == NULL || dock == NULL || name == NULL)
  {
    return false;
  }

  name_length = strlen(name);
  if (name_length == 0 || name_length >= LDK_EDITOR_DOCK_LAYOUT_NAME_CAPACITY ||
      dock->window_count > LDK_EDITOR_WINDOW_CAPACITY)
  {
    return false;
  }

  memset(layout, 0, sizeof(*layout));
  memcpy(layout->name, name, name_length + 1);
  layout->root = LDK_EDITOR_DOCK_INVALID_NODE;

  LDKEditorWindowId saved_windows[LDK_EDITOR_WINDOW_CAPACITY] = {0};
  for (u32 i = 0; i < dock->window_count; ++i)
  {
    const LDKEditorDockWindow *window = &dock->windows[i];
    const LDKUIRect *rect = &window->floating_rect;

    if (!s_editor_dock_id_list_add(saved_windows, &layout->window_count,
            LDK_EDITOR_WINDOW_CAPACITY, window->window.id) ||
        !isfinite(rect->x) || !isfinite(rect->y) || !isfinite(rect->w) ||
        !isfinite(rect->h) || rect->w <= 0.0f || rect->h <= 0.0f)
    {
      return false;
    }

    LDKEditorDockLayoutWindow *saved =
        &layout->windows[layout->window_count - 1];
    saved->id = window->window.id;
    saved->floating_rect = window->floating_rect;
  }

  if (dock->root == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return true;
  }

  memset(&context, 0, sizeof(context));
  context.dock = dock;
  context.layout = layout;
  return s_editor_dock_layout_snapshot_node(
      &context, dock->root, &layout->root);
}

static bool s_editor_dock_layout_node_write(XStrBuilder *out,
    const LDKEditorDockLayout *layout, i32 node_index, u32 indent,
    bool *visited_nodes)
{
  if (out == NULL || layout == NULL || visited_nodes == NULL ||
      node_index < 0 || (u32)node_index >= layout->node_count ||
      visited_nodes[node_index])
  {
    return false;
  }

  visited_nodes[node_index] = true;
  const LDKEditorDockLayoutNode *node = &layout->nodes[node_index];

  if (node->type == LDK_EDITOR_DOCK_NODE_LEAF)
  {
    if (node->data.leaf.window_count == 0 ||
        node->data.leaf.window_count > LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY)
    {
      return false;
    }

    s_editor_dock_tml_indent(out, indent);
    x_strbuilder_append(out, "type: \"leaf\"\n");
    s_editor_dock_tml_indent(out, indent);
    x_strbuilder_append_format(
        out, "active: %u\n", node->data.leaf.active_window);
    s_editor_dock_tml_indent(out, indent);
    x_strbuilder_append(out, "windows: [");

    for (u32 i = 0; i < node->data.leaf.window_count; ++i)
    {
      if (i > 0)
      {
        x_strbuilder_append(out, ", ");
      }

      x_strbuilder_append_format(out, "%u", node->data.leaf.windows[i]);
    }

    x_strbuilder_append(out, "]\n");
    return true;
  }

  if (node->type != LDK_EDITOR_DOCK_NODE_SPLIT ||
      !isfinite(node->data.split.ratio))
  {
    return false;
  }

  s_editor_dock_tml_indent(out, indent);
  x_strbuilder_append(out, "type: \"split\"\n");
  s_editor_dock_tml_indent(out, indent);
  x_strbuilder_append_format(out, "axis: \"%s\"\n",
      node->data.split.axis == LDK_EDITOR_DOCK_SPLIT_HORIZONTAL ? "horizontal"
                                                                : "vertical");
  s_editor_dock_tml_indent(out, indent);
  x_strbuilder_append_format(
      out, "ratio: %.9g\n", (double)node->data.split.ratio);
  s_editor_dock_tml_indent(out, indent);
  x_strbuilder_append(out, "first:\n");

  if (!s_editor_dock_layout_node_write(
          out, layout, node->data.split.first, indent + 1, visited_nodes))
  {
    return false;
  }

  s_editor_dock_tml_indent(out, indent);
  x_strbuilder_append(out, "second:\n");
  return s_editor_dock_layout_node_write(
      out, layout, node->data.split.second, indent + 1, visited_nodes);
}

static bool s_editor_dock_layout_write(
    XStrBuilder *out, const LDKEditorDockLayout *layout)
{
  bool visited_nodes[LDK_EDITOR_DOCK_NODE_CAPACITY] = {false};

  if (out == NULL || layout == NULL || layout->name[0] == 0 ||
      layout->window_count > LDK_EDITOR_WINDOW_CAPACITY ||
      layout->node_count > LDK_EDITOR_DOCK_NODE_CAPACITY)
  {
    return false;
  }

  s_editor_dock_tml_indent(out, 2);
  x_strbuilder_append(out, "- name: ");
  s_editor_dock_tml_string_append(out, layout->name);
  x_strbuilder_append_char(out, '\n');
  s_editor_dock_tml_indent(out, 3);
  x_strbuilder_append(out, "windows:\n");

  for (u32 i = 0; i < layout->window_count; ++i)
  {
    const LDKEditorDockLayoutWindow *window = &layout->windows[i];
    const LDKUIRect *rect = &window->floating_rect;

    if (window->id == LDK_EDITOR_WINDOW_ID_INVALID || !isfinite(rect->x) ||
        !isfinite(rect->y) || !isfinite(rect->w) || !isfinite(rect->h) ||
        rect->w <= 0.0f || rect->h <= 0.0f)
    {
      return false;
    }

    s_editor_dock_tml_indent(out, 4);
    x_strbuilder_append_format(out, "- id: %u\n", window->id);
    s_editor_dock_tml_indent(out, 5);
    x_strbuilder_append_format(out,
        "floating_rect: [%.9g, %.9g, %.9g, %.9g]\n",
        (double)rect->x, (double)rect->y, (double)rect->w, (double)rect->h);
  }

  s_editor_dock_tml_indent(out, 3);
  x_strbuilder_append(out, "tree:\n");

  if (layout->root == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    s_editor_dock_tml_indent(out, 4);
    x_strbuilder_append(out, "type: \"empty\"\n");
    return true;
  }

  return s_editor_dock_layout_node_write(
      out, layout, layout->root, 4, visited_nodes);
}

static bool s_editor_dock_window_ids_from_entry(const TMLDocument *document,
    const TMLEntry *entry, LDKEditorWindowId *windows, u32 *window_count)
{
  if (document == NULL || entry == NULL || windows == NULL ||
      window_count == NULL)
  {
    return false;
  }

  *window_count = 0;

  if (entry->type == TML_VALUE_I64)
  {
    if (!s_editor_dock_window_id_from_entry(entry, &windows[0]))
    {
      return false;
    }

    *window_count = 1;
    return true;
  }

  if (entry->type != TML_VALUE_ARRAY_I64)
  {
    return false;
  }

  TMLI64Slice array;
  if (!tml_entry_get_i64_array(document, entry, &array) || array.count == 0 ||
      array.count > LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY)
  {
    return false;
  }

  for (u32 i = 0; i < array.count; ++i)
  {
    if (array.data[i] <= 0 || array.data[i] > UINT32_MAX)
    {
      return false;
    }

    windows[i] = (LDKEditorWindowId)array.data[i];
  }

  *window_count = array.count;
  return true;
}

static bool s_editor_dock_layout_windows_read(
    LDKEditorDockLayoutReadContext *context, const TMLNode *windows_node)
{
  if (context == NULL || windows_node == NULL ||
      windows_node->child_count > LDK_EDITOR_WINDOW_CAPACITY)
  {
    return false;
  }

  for (u32 i = 0; i < windows_node->child_count; ++i)
  {
    const TMLNode *window_node =
        tml_node_child_at(context->document, windows_node, i);
    const TMLEntry *id_entry =
        tml_node_find_entry(context->document, window_node, "id");
    const TMLEntry *rect_entry =
        tml_node_find_entry(context->document, window_node, "floating_rect");
    LDKEditorWindowId id;
    LDKUIRect rect;

    if (!s_editor_dock_window_id_from_entry(id_entry, &id) ||
        !s_editor_dock_rect_from_entry(context->document, rect_entry, &rect) ||
        !s_editor_dock_id_list_add(context->saved_windows,
            &context->saved_window_count, LDK_EDITOR_WINDOW_CAPACITY, id))
    {
      return false;
    }

    if (s_editor_dock_window_get_const(context->dock, id) == NULL)
    {
      continue;
    }

    LDKEditorDockLayoutWindow *window =
        &context->layout->windows[context->layout->window_count++];
    window->id = id;
    window->floating_rect = rect;
  }

  return true;
}

static bool s_editor_dock_layout_node_read(
    LDKEditorDockLayoutReadContext *context, const TMLNode *source_node,
    u32 depth, i32 *layout_node)
{
  TMLString type;

  if (context == NULL || source_node == NULL || layout_node == NULL ||
      depth > LDK_EDITOR_DOCK_NODE_CAPACITY ||
      !tml_node_get_string(context->document, source_node, "type", &type))
  {
    return false;
  }

  context->source_node_count += 1;
  if (context->source_node_count > LDK_EDITOR_DOCK_NODE_CAPACITY)
  {
    return false;
  }

  if (s_editor_dock_tml_string_equals(type, "empty"))
  {
    *layout_node = LDK_EDITOR_DOCK_INVALID_NODE;
    return true;
  }

  if (s_editor_dock_tml_string_equals(type, "leaf"))
  {
    const TMLEntry *windows_entry =
        tml_node_find_entry(context->document, source_node, "windows");
    const TMLEntry *active_entry =
        tml_node_find_entry(context->document, source_node, "active");
    LDKEditorWindowId saved_leaf_windows[LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY];
    LDKEditorWindowId active_window;
    u32 saved_leaf_window_count;

    if (!s_editor_dock_window_ids_from_entry(context->document, windows_entry,
            saved_leaf_windows, &saved_leaf_window_count) ||
        !s_editor_dock_window_id_from_entry(active_entry, &active_window))
    {
      return false;
    }

    LDKEditorWindowId leaf_windows[LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY];
    u32 leaf_window_count = 0;

    for (u32 i = 0; i < saved_leaf_window_count; ++i)
    {
      LDKEditorWindowId window = saved_leaf_windows[i];

      if (!s_editor_dock_id_list_contains(
              context->saved_windows, context->saved_window_count, window) ||
          !s_editor_dock_id_list_add(context->docked_windows,
              &context->docked_window_count, LDK_EDITOR_WINDOW_CAPACITY,
              window))
      {
        return false;
      }

      if (s_editor_dock_window_get_const(context->dock, window) != NULL)
      {
        leaf_windows[leaf_window_count++] = window;
      }
    }

    if (leaf_window_count == 0)
    {
      *layout_node = LDK_EDITOR_DOCK_INVALID_NODE;
      return true;
    }

    if (context->layout->node_count >= LDK_EDITOR_DOCK_NODE_CAPACITY)
    {
      return false;
    }

    i32 node_index = (i32)context->layout->node_count++;
    LDKEditorDockLayoutNode *node = &context->layout->nodes[node_index];
    node->type = LDK_EDITOR_DOCK_NODE_LEAF;
    node->data.leaf.window_count = leaf_window_count;

    for (u32 i = 0; i < leaf_window_count; ++i)
    {
      node->data.leaf.windows[i] = leaf_windows[i];
    }

    node->data.leaf.active_window = s_editor_dock_id_list_contains(leaf_windows,
                                        leaf_window_count, active_window)
                                        ? active_window
                                        : leaf_windows[0];

    *layout_node = node_index;
    return true;
  }

  if (!s_editor_dock_tml_string_equals(type, "split"))
  {
    return false;
  }

  TMLString axis;
  const TMLEntry *ratio_entry =
      tml_node_find_entry(context->document, source_node, "ratio");
  const TMLNode *first_node =
      tml_node_find_child(context->document, source_node, "first");
  const TMLNode *second_node =
      tml_node_find_child(context->document, source_node, "second");
  float ratio;
  LDKEditorDockSplitAxis split_axis;
  i32 first;
  i32 second;

  if (!tml_node_get_string(context->document, source_node, "axis", &axis) ||
      !s_editor_dock_float_from_entry(ratio_entry, &ratio) ||
      first_node == NULL || second_node == NULL)
  {
    return false;
  }

  if (s_editor_dock_tml_string_equals(axis, "horizontal"))
  {
    split_axis = LDK_EDITOR_DOCK_SPLIT_HORIZONTAL;
  }
  else if (s_editor_dock_tml_string_equals(axis, "vertical"))
  {
    split_axis = LDK_EDITOR_DOCK_SPLIT_VERTICAL;
  }
  else
  {
    return false;
  }

  if (!s_editor_dock_layout_node_read(context, first_node, depth + 1, &first) ||
      !s_editor_dock_layout_node_read(context, second_node, depth + 1, &second))
  {
    return false;
  }

  if (first == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    *layout_node = second;
    return true;
  }

  if (second == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    *layout_node = first;
    return true;
  }

  if (context->layout->node_count >= LDK_EDITOR_DOCK_NODE_CAPACITY)
  {
    return false;
  }

  i32 node_index = (i32)context->layout->node_count++;
  LDKEditorDockLayoutNode *node = &context->layout->nodes[node_index];
  node->type = LDK_EDITOR_DOCK_NODE_SPLIT;
  node->data.split.first = first;
  node->data.split.second = second;
  node->data.split.ratio = s_editor_dock_clampf(
      ratio, LDK_EDITOR_DOCK_SIZE_MIN, LDK_EDITOR_DOCK_SIZE_MAX);
  node->data.split.axis = split_axis;
  *layout_node = node_index;
  return true;
}

static bool s_editor_dock_layout_read(const TMLDocument *document,
    const TMLNode *layout_node, const LDKEditorDockState *dock,
    LDKEditorDockLayout *layout)
{
  LDKEditorDockLayoutReadContext context;
  const TMLNode *windows_node;
  const TMLNode *tree_node;
  TMLString name;

  if (document == NULL || layout_node == NULL || dock == NULL ||
      layout == NULL ||
      !tml_node_get_string(document, layout_node, "name", &name))
  {
    return false;
  }

  memset(layout, 0, sizeof(*layout));
  layout->root = LDK_EDITOR_DOCK_INVALID_NODE;

  if (!s_editor_dock_layout_name_copy(layout->name, sizeof(layout->name), name))
  {
    return false;
  }

  windows_node = tml_node_find_child(document, layout_node, "windows");
  tree_node = tml_node_find_child(document, layout_node, "tree");
  if (windows_node == NULL || tree_node == NULL)
  {
    return false;
  }

  memset(&context, 0, sizeof(context));
  context.document = document;
  context.dock = dock;
  context.layout = layout;

  if (!s_editor_dock_layout_windows_read(&context, windows_node) ||
      !s_editor_dock_layout_node_read(&context, tree_node, 0, &layout->root))
  {
    return false;
  }

  return true;
}

static i32 s_editor_dock_layout_apply_node(LDKEditorDockState *dock,
    const LDKEditorDockLayout *layout, i32 layout_node)
{
  if (dock == NULL || layout == NULL || layout_node < 0 ||
      (u32)layout_node >= layout->node_count)
  {
    return LDK_EDITOR_DOCK_INVALID_NODE;
  }

  const LDKEditorDockLayoutNode *node = &layout->nodes[layout_node];
  if (node->type == LDK_EDITOR_DOCK_NODE_LEAF)
  {
    LDKEditorWindowId windows[LDK_EDITOR_DOCK_LEAF_WINDOW_CAPACITY];
    u32 window_count = 0;

    for (u32 i = 0; i < node->data.leaf.window_count; ++i)
    {
      LDKEditorWindowId window = node->data.leaf.windows[i];
      if (s_editor_dock_window_get(dock, window) != NULL)
      {
        windows[window_count++] = window;
      }
    }

    if (window_count == 0)
    {
      return LDK_EDITOR_DOCK_INVALID_NODE;
    }

    LDKEditorWindowId active_window =
        s_editor_dock_id_list_contains(
            windows, window_count, node->data.leaf.active_window)
            ? node->data.leaf.active_window
            : windows[0];

    return s_editor_dock_leaf_create_tabs(
        dock, windows, window_count, active_window);
  }

  if (node->type != LDK_EDITOR_DOCK_NODE_SPLIT)
  {
    return LDK_EDITOR_DOCK_INVALID_NODE;
  }

  i32 first =
      s_editor_dock_layout_apply_node(dock, layout, node->data.split.first);
  i32 second =
      s_editor_dock_layout_apply_node(dock, layout, node->data.split.second);

  if (first == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return second;
  }

  if (second == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return first;
  }

  return s_editor_dock_split_create(
      dock, node->data.split.axis, node->data.split.ratio, first, second);
}

static bool s_editor_dock_layout_apply(
    LDKEditorDockState *dock, const LDKEditorDockLayout *layout)
{
  if (dock == NULL || layout == NULL)
  {
    return false;
  }

  LDKEditorDockState candidate = *dock;
  memset(candidate.nodes, 0, sizeof(candidate.nodes));
  candidate.root = LDK_EDITOR_DOCK_INVALID_NODE;
  candidate.target_overlay_window_id = 0;
  s_editor_dock_drag_reset(&candidate.drag);
  s_editor_dock_resize_reset(&candidate.resize);

  for (u32 i = 0; i < candidate.window_count; ++i)
  {
    candidate.windows[i].leaf = LDK_EDITOR_DOCK_INVALID_NODE;
    candidate.windows[i].ui_window_id = 0;
  }

  for (u32 i = 0; i < layout->window_count; ++i)
  {
    LDKEditorDockWindow *window =
        s_editor_dock_window_get(&candidate, layout->windows[i].id);
    if (window != NULL)
    {
      window->floating_rect = layout->windows[i].floating_rect;
    }
  }

  if (layout->root != LDK_EDITOR_DOCK_INVALID_NODE)
  {
    candidate.root =
        s_editor_dock_layout_apply_node(&candidate, layout, layout->root);
    if (candidate.root == LDK_EDITOR_DOCK_INVALID_NODE)
    {
      return false;
    }

    candidate.nodes[candidate.root].parent = LDK_EDITOR_DOCK_INVALID_NODE;
  }

  s_editor_dock_window_locations_refresh(&candidate);
  *dock = candidate;
  return true;
}

/*
 * Serializes all named dock layouts to TML. Before writing, the currently
 * selected layout is refreshed from the live dock state.
 */
static bool ldk_editor_dock_to_tml(XStrBuilder *out)
{
  LDKEditorDockLayouts layouts = s_editor_dock_layouts;
  LDKEditorDockLayout current_layout;
  const char *current_name;

  if (out == NULL || !s_editor_dock.initialized)
  {
    return false;
  }

  if (layouts.layout_count == 0)
  {
    layouts.layout_count = 1;
    layouts.current_layout = 0;
    current_name = "default";
  }
  else
  {
    if (layouts.layout_count > LDK_EDITOR_DOCK_LAYOUT_CAPACITY ||
        layouts.current_layout >= layouts.layout_count)
    {
      return false;
    }

    current_name = layouts.layouts[layouts.current_layout].name;
  }

  if (!s_editor_dock_layout_snapshot(
          &current_layout, &s_editor_dock, current_name))
  {
    return false;
  }

  layouts.layouts[layouts.current_layout] = current_layout;

  for (u32 i = 0; i < layouts.layout_count; ++i)
  {
    for (u32 j = i + 1; j < layouts.layout_count; ++j)
    {
      if (strcmp(layouts.layouts[i].name, layouts.layouts[j].name) == 0)
      {
        return false;
      }
    }
  }

  x_strbuilder_clear(out);
  x_strbuilder_append(out, "dock:\n");
  x_strbuilder_append_format(
      out, "  version: %u\n", LDK_EDITOR_DOCK_TML_VERSION);
  x_strbuilder_append(out, "  current: ");
  s_editor_dock_tml_string_append(
      out, layouts.layouts[layouts.current_layout].name);
  x_strbuilder_append_char(out, '\n');
  x_strbuilder_append(out, "  layouts:\n");

  for (u32 i = 0; i < layouts.layout_count; ++i)
  {
    if (!s_editor_dock_layout_write(out, &layouts.layouts[i]))
    {
      x_strbuilder_clear(out);
      return false;
    }
  }

  s_editor_dock_layouts = layouts;
  return true;
}

/*
 * Deserializes a named layout collection and applies its selected layout.
 * The live dock and the stored layouts are only replaced after the complete
 * document has been validated.
 */
static bool ldk_editor_dock_from_tml(const char *source)
{
  TMLParseResult parse;
  const TMLNode *dock_node;
  const TMLNode *layouts_node;
  TMLDocument *document;
  LDKEditorDockLayouts layouts;
  TMLString current_name = {0};
  i64 version;
  bool has_current;
  bool ok = false;

  if (source == NULL || !s_editor_dock.initialized)
  {
    return false;
  }

  parse = tml_parse(source);
  if (!parse.ok)
  {
    return false;
  }

  document = parse.document;
  memset(&layouts, 0, sizeof(layouts));
  layouts.current_layout = LDK_EDITOR_DOCK_INVALID_LAYOUT;

  dock_node = tml_path_find_node(document, "dock");
  if (dock_node == NULL ||
      !tml_node_get_i64(document, dock_node, "version", &version) ||
      version != LDK_EDITOR_DOCK_TML_VERSION)
  {
    goto cleanup;
  }

  has_current =
      tml_node_get_string(document, dock_node, "current", &current_name) != 0;
  layouts_node = tml_node_find_child(document, dock_node, "layouts");
  if (layouts_node == NULL || layouts_node->child_count == 0 ||
      layouts_node->child_count > LDK_EDITOR_DOCK_LAYOUT_CAPACITY)
  {
    goto cleanup;
  }

  for (u32 i = 0; i < layouts_node->child_count; ++i)
  {
    const TMLNode *layout_node = tml_node_child_at(document, layouts_node, i);
    LDKEditorDockLayout *layout = &layouts.layouts[layouts.layout_count];

    if (!s_editor_dock_layout_read(
            document, layout_node, &s_editor_dock, layout))
    {
      goto cleanup;
    }

    for (u32 j = 0; j < layouts.layout_count; ++j)
    {
      if (strcmp(layouts.layouts[j].name, layout->name) == 0)
      {
        goto cleanup;
      }
    }

    if (has_current &&
        s_editor_dock_tml_string_equals(current_name, layout->name))
    {
      layouts.current_layout = layouts.layout_count;
    }

    layouts.layout_count += 1;
  }

  if (layouts.current_layout == LDK_EDITOR_DOCK_INVALID_LAYOUT)
  {
    for (u32 i = 0; i < layouts.layout_count; ++i)
    {
      if (strcmp(layouts.layouts[i].name, "default") == 0)
      {
        layouts.current_layout = i;
        break;
      }
    }
  }

  if (layouts.current_layout == LDK_EDITOR_DOCK_INVALID_LAYOUT)
  {
    layouts.current_layout = 0;
  }

  if (!s_editor_dock_layout_apply(
          &s_editor_dock, &layouts.layouts[layouts.current_layout]))
  {
    goto cleanup;
  }

  s_editor_dock_layouts = layouts;
  ok = true;

cleanup:
  tml_document_free(document);
  return ok;
}

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                 */
/* ------------------------------------------------------------------------- */

static void s_editor_dock_layout_reset_preserving_windows(
  LDKEditorDockState *dock)
{
  LDKEditorDockWindow windows[LDK_EDITOR_WINDOW_CAPACITY];
  u32 window_count = dock->window_count;

  if (window_count > 0)
  {
    memcpy(windows, dock->windows,
      sizeof(LDKEditorDockWindow) * window_count);
  }

  *dock = (LDKEditorDockState){
    .root = LDK_EDITOR_DOCK_INVALID_NODE
  };

  if (window_count > 0)
  {
    memcpy(dock->windows, windows,
      sizeof(LDKEditorDockWindow) * window_count);
  }

  dock->window_count = window_count;
  dock->project_explorer.root_expanded = true;
  dock->project_explorer.icon_size = 48.0f;
  dock->inspector.enabled = true;

  for (u32 i = 0; i < dock->window_count; ++i)
  {
    dock->windows[i].leaf = LDK_EDITOR_DOCK_INVALID_NODE;
    dock->windows[i].ui_window_id = 0;
  }

  s_editor_dock_drag_reset(&dock->drag);
  s_editor_dock_resize_reset(&dock->resize);
}

static bool ldk_editor_dock_init(LDKEditorContext *editor)
{
  if (editor == NULL)
  {
    return false;
  }

  LDKEditorDockState *dock = &s_editor_dock;
  s_editor_dock_layout_reset_preserving_windows(dock);

  if (!s_editor_builtin_windows_add(editor, dock))
  {
    return false;
  }

  i32 project_leaf = s_editor_dock_leaf_create(
    dock, LDK_EDITOR_WINDOW_PROJECT_EXPLORER);
  i32 scene_leaf = s_editor_dock_leaf_create(
    dock, LDK_EDITOR_WINDOW_SCENE);

  const LDKEditorWindowId bottom_windows[] = {
    LDK_EDITOR_WINDOW_INSPECTOR,
    LDK_EDITOR_WINDOW_CONSOLE
  };

  i32 bottom_leaf = s_editor_dock_leaf_create_tabs(
    dock,
    bottom_windows,
    (u32)(sizeof(bottom_windows) / sizeof(bottom_windows[0])),
    LDK_EDITOR_WINDOW_INSPECTOR);

  if (project_leaf == LDK_EDITOR_DOCK_INVALID_NODE ||
      scene_leaf == LDK_EDITOR_DOCK_INVALID_NODE ||
      bottom_leaf == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return false;
  }

  i32 right_split = s_editor_dock_split_create(dock,
    LDK_EDITOR_DOCK_SPLIT_VERTICAL, 0.66f, scene_leaf, bottom_leaf);

  if (right_split == LDK_EDITOR_DOCK_INVALID_NODE)
  {
    return false;
  }

  i32 root = s_editor_dock_split_create(dock,
    LDK_EDITOR_DOCK_SPLIT_HORIZONTAL, 0.25f, project_leaf, right_split);

  if (root == LDK_EDITOR_DOCK_INVALID_NODE)
  {
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
  s_editor_dock_layouts = (LDKEditorDockLayouts){
    .current_layout = LDK_EDITOR_DOCK_INVALID_LAYOUT
  };
}
