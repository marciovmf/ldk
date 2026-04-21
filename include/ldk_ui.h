#ifndef LDK_UI_H
#define LDK_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ldk_common.h>
#include <ldk_os.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_arena.h>

#include <stdbool.h>
#include <stdint.h>

  typedef uint32_t LDKUIId;
  typedef uintptr_t LDKUITextureHandle;
  typedef struct LDKUIItem LDKUIItem;
  typedef struct LDKUILayoutNode LDKUILayoutNode;
  typedef struct LDKUIWindow LDKUIWindow;
  typedef struct LDKUIContext LDKUIContext;

  typedef struct LDKUIVertex
  {
    float x;
    float y;
    float u;
    float v;
    u32 color;
  } LDKUIVertex;

  typedef struct LDKUIRect
  {
    float x;
    float y;
    float w;
    float h;
  } LDKUIRect;

  typedef struct LDKUIDrawCmd
  {
    LDKUITextureHandle texture;
    LDKUIRect clip_rect;
    u32 index_offset;
    u32 index_count;
  } LDKUIDrawCmd;

  typedef struct LDKUIWidgetState
  {
    LDKUIId id;
    LDKUIRect rect;
  } LDKUIWidgetState;

  struct LDKUIWindow
  {
    LDKUIId id;
    char title[64];
    LDKUIRect rect;
    LDKUIRect title_bar_rect;
    LDKUIRect content_rect;
    bool is_open;
    bool is_hovered;
    bool is_focused;
    bool is_active;
    bool is_dragging;
    float drag_offset_x;
    float drag_offset_y;
    uint32_t z_order;
    LDKUILayoutNode* root_layout;
  };

  X_ARRAY_TYPE_NAMED(LDKUIWindow, ldk_ui_window);
  X_ARRAY_TYPE_NAMED(LDKUIId, ldk_ui_id);
  X_ARRAY_TYPE_NAMED(LDKUIVertex, ldk_ui_vertex);
  X_ARRAY_TYPE_NAMED(uint32_t, ldk_ui_u32);
  X_ARRAY_TYPE_NAMED(LDKUIDrawCmd, ldk_ui_draw_cmd);
  X_ARRAY_TYPE_NAMED(LDKUIItem*, ldk_ui_item_ptr);
  X_ARRAY_TYPE_NAMED(LDKUIWidgetState, ldk_ui_widget_state);

  typedef struct LDKUISize
  {
    float w;
    float h;
  } LDKUISize;

  typedef struct LDKUIRenderData
  {
    LDKUIVertex const* vertices;
    u32 vertex_count;
    u32 const* indices;
    u32 index_count;
    LDKUIDrawCmd const* commands;
    uint32_t command_count;
  } LDKUIRenderData;

  typedef struct LDKUIConfig
  {
    u32 frame_arena_size;
    u32 initial_vertex_capacity;
    u32 initial_index_capacity;
    u32 initial_command_capacity;
    u32 initial_window_capacity;
    u32 initial_id_stack_capacity;
  } LDKUIConfig;

  typedef enum LDKUILayoutDirection
  {
    LDK_UI_LAYOUT_VERTICAL = 0,
    LDK_UI_LAYOUT_HORIZONTAL = 1
  } LDKUILayoutDirection;

  typedef enum LDKUIItemType
  {
    LDK_UI_ITEM_LABEL = 0,
    LDK_UI_ITEM_BUTTON = 1,
    LDK_UI_ITEM_TOGGLE_BUTTON = 2,
    LDK_UI_ITEM_SLIDER_FLOAT = 3,
    LDK_UI_ITEM_LAYOUT = 4
  } LDKUIItemType;

  typedef struct LDKUINextLayout
  {
    bool has_width;
    bool has_height;
    bool has_min_width;
    bool has_min_height;
    bool has_expand_width;
    bool has_expand_height;
    float width;
    float height;
    float min_width;
    float min_height;
    float max_width;
    float max_height;
    bool has_max_width;
    bool has_max_height;
    bool expand_width;
    bool expand_height;
  } LDKUINextLayout;

  struct LDKUIItem
  {
    LDKUIId id;
    LDKUIItemType type;
    LDKUIRect rect;
    float preferred_width;
    float preferred_height;
    float min_width;
    float min_height;
    float max_width;
    float max_height;
    bool has_max_width;
    bool has_max_height;
    bool expand_width;
    bool expand_height;
    bool clicked;
    bool changed;
    char const* text;
    union
    {
      struct
      {
        bool* value;
      } toggle_button;
      struct
      {
        float* value;
        float min_value;
        float max_value;
      } slider_float;
      struct
      {
        LDKUILayoutNode* node;
      } layout;
    } data;
  };

  struct LDKUILayoutNode
  {
    LDKUIId id;
    u32 item_count;
    LDKUILayoutDirection direction;
    LDKUIRect rect;
    float spacing;
    float padding;
    XArray_ldk_ui_item_ptr* items;
    LDKUILayoutNode* parent;
    LDKUIWindow* window;
  };

  struct LDKUIContext
  {
    XArena* frame_arena;
    LDKMouseState const* mouse;
    LDKKeyboardState const* keyboard;
    LDKUIRect viewport;
    u32 root_item_count;
    LDKUIId hot_id;
    LDKUIId active_id;
    LDKUIId focused_id;
    LDKUIId last_id;
    LDKUIWindow* hovered_window;
    LDKUIWindow* focused_window;
    LDKUIWindow* active_window;
    LDKUIWindow* current_window;
    LDKUILayoutNode* current_layout;
    LDKUINextLayout next_layout;
    XArray_ldk_ui_window* windows;
    XArray_ldk_ui_id* id_stack;
    XArray_ldk_ui_vertex* vertices;
    XArray_ldk_ui_u32* indices;
    XArray_ldk_ui_draw_cmd* commands;
    XArray_ldk_ui_widget_state* widget_states;
    LDKUIRenderData render_data;
  };

  // lifecycle
  bool ldk_ui_initialize(LDKUIContext* ctx, LDKUIConfig const* config);
  void ldk_ui_terminate(LDKUIContext* ctx);

  // frame
  void ldk_ui_begin_frame(LDKUIContext* ctx, LDKMouseState const* mouse, LDKKeyboardState const* keyboard, LDKUIRect viewport);
  void ldk_ui_end_frame(LDKUIContext* ctx);
  LDKUIRenderData const* ldk_ui_get_render_data(LDKUIContext const* ctx);

  // id stack
  void ldk_ui_push_id_u32(LDKUIContext* ctx, uint32_t value);
  void ldk_ui_push_id_ptr(LDKUIContext* ctx, void const* value);
  void ldk_ui_push_id_cstr(LDKUIContext* ctx, char const* value);
  void ldk_ui_pop_id(LDKUIContext* ctx);

  // next-item layout hints
  void ldk_ui_set_next_width(LDKUIContext* ctx, float width);
  void ldk_ui_set_next_height(LDKUIContext* ctx, float height);
  void ldk_ui_set_next_min_width(LDKUIContext* ctx, float width);
  void ldk_ui_set_next_min_height (LDKUIContext* ctx, float height);
  void ldk_ui_set_next_control_max_size(LDKUIContext* ctx, float width, float height);
  void ldk_ui_set_next_expand_width(LDKUIContext* ctx, bool expand);
  void ldk_ui_set_next_expand_height(LDKUIContext* ctx, bool expand);

  // layout
  void ldk_ui_begin_vertical(LDKUIContext* ctx);
  void ldk_ui_end_vertical(LDKUIContext* ctx);
  void ldk_ui_begin_horizontal(LDKUIContext* ctx);
  void ldk_ui_end_horizontal(LDKUIContext* ctx);


  // Pane
  bool ldk_ui_begin_pane(LDKUIContext* ctx, LDKUIRect rect);
  bool ldk_ui_begin_pane_ex(
      LDKUIContext* ctx,
      char const* id,
      char const* title,
      LDKUIRect rect,
      bool toolbar,
      bool draggable);
  void ldk_ui_end_pane(LDKUIContext* ctx);


  // windows
  bool ldk_ui_begin_window(LDKUIContext* ctx, char const* title, LDKUIRect rect);
  bool ldk_ui_begin_window_ex(LDKUIContext* ctx, char const* id, char const* title, LDKUIRect rect);
  void ldk_ui_end_window(LDKUIContext* ctx);

  // widgets
  void ldk_ui_label(LDKUIContext* ctx, char const* text);
  bool ldk_ui_button(LDKUIContext* ctx, char const* text);
  bool ldk_ui_button_ex(LDKUIContext* ctx, char const* id, char const* text);
  bool ldk_ui_toggle_button(LDKUIContext* ctx, char const* text, bool* value);
  bool ldk_ui_toggle_button_ex(LDKUIContext* ctx, char const* id, char const* text, bool* value);
  bool ldk_ui_slider_float(LDKUIContext* ctx, char const* text, float* value, float min_value, float max_value);
  bool ldk_ui_slider_float_ex(LDKUIContext* ctx, char const* id, char const* text, float* value, float min_value, float max_value);



  // text input
  void ldk_ui_input_text(LDKUIContext* ctx, u32 codepoint);

#ifdef __cplusplus
}
#endif

#endif
