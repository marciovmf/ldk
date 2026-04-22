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
    LDKUIItem* next_sibling;
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
    //u32 item_count;
    u32 child_count;
    LDKUILayoutDirection direction;
    LDKUIRect rect;
    float spacing;
    float padding;
    LDKUIItem* first_item;
    LDKUIItem* last_item;
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
  LDK_API bool ldk_ui_initialize(LDKUIContext* ctx, LDKUIConfig const* config);
  LDK_API void ldk_ui_terminate(LDKUIContext* ctx);

  // frame
  LDK_API void ldk_ui_begin_frame(LDKUIContext* ctx, LDKMouseState const* mouse, LDKKeyboardState const* keyboard, LDKUIRect viewport);
  LDK_API void ldk_ui_end_frame(LDKUIContext* ctx);
  LDK_API LDKUIRenderData const* ldk_ui_get_render_data(LDKUIContext const* ctx);

  // id stack
  LDK_API void ldk_ui_push_id_u32(LDKUIContext* ctx, uint32_t value);
  LDK_API void ldk_ui_push_id_ptr(LDKUIContext* ctx, void const* value);
  LDK_API void ldk_ui_push_id_cstr(LDKUIContext* ctx, char const* value);
  LDK_API void ldk_ui_pop_id(LDKUIContext* ctx);

  // next-item layout hints
  LDK_API void ldk_ui_set_next_width(LDKUIContext* ctx, float width);
  LDK_API void ldk_ui_set_next_height(LDKUIContext* ctx, float height);
  LDK_API void ldk_ui_set_next_min_width(LDKUIContext* ctx, float width);
  LDK_API void ldk_ui_set_next_min_height (LDKUIContext* ctx, float height);
  LDK_API void ldk_ui_set_next_control_max_size(LDKUIContext* ctx, float width, float height);
  LDK_API void ldk_ui_set_next_expand_width(LDKUIContext* ctx, bool expand);
  LDK_API void ldk_ui_set_next_expand_height(LDKUIContext* ctx, bool expand);

  // layout
  LDK_API void ldk_ui_begin_vertical(LDKUIContext* ctx);
  LDK_API void ldk_ui_end_vertical(LDKUIContext* ctx);
  LDK_API void ldk_ui_begin_horizontal(LDKUIContext* ctx);
  LDK_API void ldk_ui_end_horizontal(LDKUIContext* ctx);


  // Pane
  LDK_API bool ldk_ui_begin_pane(LDKUIContext* ctx, LDKUIRect rect);
  LDK_API bool ldk_ui_begin_pane_ex(LDKUIContext* ctx, char const* id, char const* title, LDKUIRect rect, bool toolbar, bool draggable);
  LDK_API void ldk_ui_end_pane(LDKUIContext* ctx);


  // windows
  LDK_API bool ldk_ui_begin_window(LDKUIContext* ctx, char const* title, LDKUIRect rect);
  LDK_API bool ldk_ui_begin_window_ex(LDKUIContext* ctx, char const* id, char const* title, LDKUIRect rect);
  LDK_API void ldk_ui_end_window(LDKUIContext* ctx);

  // widgets
  LDK_API void ldk_ui_label(LDKUIContext* ctx, char const* text);
  LDK_API bool ldk_ui_button(LDKUIContext* ctx, char const* text);
  LDK_API bool ldk_ui_button_ex(LDKUIContext* ctx, char const* id, char const* text);
  LDK_API bool ldk_ui_toggle_button(LDKUIContext* ctx, char const* text, bool* value);
  LDK_API bool ldk_ui_toggle_button_ex(LDKUIContext* ctx, char const* id, char const* text, bool* value);
  LDK_API bool ldk_ui_slider_float(LDKUIContext* ctx, char const* text, float* value, float min_value, float max_value);
  LDK_API bool ldk_ui_slider_float_ex(LDKUIContext* ctx, char const* id, char const* text, float* value, float min_value, float max_value);

  // text input
  LDK_API void ldk_ui_input_text(LDKUIContext* ctx, u32 codepoint);

#ifdef __cplusplus
}
#endif

#endif // LDK_UI_H
