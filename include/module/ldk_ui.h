#ifndef LDK_UI_H
#define LDK_UI_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <ldk_common.h>
#include <ldk_os.h>
#include <ldk_ttf.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_arena.h>

#include <stdbool.h>
#include <stdint.h>

#ifndef LDK_UI_INPUT_CODEPOINTS_CAPACITY
#define LDK_UI_INPUT_CODEPOINTS_CAPACITY 16
#endif

#ifndef LDK_UI_DEFAULT_CONTROL_HEIGHT
#define LDK_UI_DEFAULT_CONTROL_HEIGHT 22.0f
#endif

#ifndef LDK_UI_LAYOUT_STACK_CAPACITY
#define LDK_UI_LAYOUT_STACK_CAPACITY 64
#endif

#ifndef LDK_UI_LAYOUT_ITEM_CAPACITY
#define LDK_UI_LAYOUT_ITEM_CAPACITY 512
#endif

#ifndef LDK_UI_LAYOUT_ITEM_CACHE_CAPACITY
#define LDK_UI_LAYOUT_ITEM_CACHE_CAPACITY 64
#endif

#ifndef LDK_UI_MEASURE_ENTRY_CAPACITY
#define LDK_UI_MEASURE_ENTRY_CAPACITY 64
#endif

#ifndef LDK_UI_WINDOW_CAPACITY
#define LDK_UI_WINDOW_CAPACITY 16
#endif

#ifndef LDK_UI_WINDOW_STACK_CAPACITY
#define LDK_UI_WINDOW_STACK_CAPACITY 16
#endif

#ifndef LDK_UI_SCROLLVIEW_STACK_CAPACITY
#define LDK_UI_SCROLLVIEW_STACK_CAPACITY 16
#endif

#ifndef LDK_UI_SCROLLVIEW_CACHE_CAPACITY
#define LDK_UI_SCROLLVIEW_CACHE_CAPACITY 16
#endif

#ifndef LDK_UI_SCROLLBAR_SIZE
#define LDK_UI_SCROLLBAR_SIZE 12.0f
#endif

#ifndef LDK_UI_AREA_STACK_CAPACITY
#define LDK_UI_AREA_STACK_CAPACITY 64
#endif

#ifndef LDK_UI_POPUP_STACK_CAPACITY
#define LDK_UI_POPUP_STACK_CAPACITY 16
#endif

#ifndef LDK_UI_MIN_WINDOW_WIDTH
#define LDK_UI_MIN_WINDOW_WIDTH 64.0f
#endif

#ifndef LDK_UI_MIN_WINDOW_HEIGHT
#define LDK_UI_MIN_WINDOW_HEIGHT 1
#endif

#ifndef LDK_UI_TITLE_BAR_HEIGHT
#define LDK_UI_TITLE_BAR_HEIGHT 24.0f
#endif

#ifndef LDK_UI_DEFAULT_SPACING
#define LDK_UI_DEFAULT_SPACING 4.0f
#endif

#ifndef LDK_UI_DEFAULT_PADDING
#define LDK_UI_DEFAULT_PADDING 4.0f
#endif

  typedef uint32_t LDKUIId;
  typedef uintptr_t LDKUITextureHandle;
  typedef LDKUITextureHandle (*LDKUIGetFontPageTextureFn)(
      void *user, LDKFontInstance *font, u32 page_index);
  typedef u32 LDKUIColor;
  typedef LDKPointf LDKUIPoint;
  typedef LDKSizef LDKUISize;
  typedef LDKRectf LDKUIRect;

  typedef struct LDKUILayout LDKUILayout;
  typedef struct LDKUIWindow LDKUIWindow;
  typedef struct LDKUIWindowStackEntry LDKUIWindowStackEntry;
  typedef struct LDKUIScrollViewCache LDKUIScrollViewCache;
  typedef struct LDKUIScrollViewStackEntry LDKUIScrollViewStackEntry;
  typedef struct LDKUIAreaStackEntry LDKUIAreaStackEntry;
  typedef struct LDKUIPopupStackEntry LDKUIPopupStackEntry;
  typedef struct LDKUIPopupFrameEntry LDKUIPopupFrameEntry;
  typedef struct LDKUIHitCandidate LDKUIHitCandidate;
  typedef struct LDKUIContext LDKUIContext;

  typedef enum LDKUIColorSlot
  {
    LDK_UI_COLOR_TEXT,
    LDK_UI_COLOR_TEXT_DISABLED,
    LDK_UI_COLOR_WINDOW_BG,
    LDK_UI_COLOR_PANEL_BG,
    LDK_UI_COLOR_CONTROL_BG,
    LDK_UI_COLOR_CONTROL_BG_HOVERED,
    LDK_UI_COLOR_CONTROL_BG_ACTIVE,
    LDK_UI_COLOR_CONTROL_BG_ACTIVE_HOVERED,
    LDK_UI_COLOR_CONTROL_TEXT,
    LDK_UI_COLOR_CONTROL_TEXT_HOVERED,
    LDK_UI_COLOR_CONTROL_TEXT_ACTIVE,
    LDK_UI_COLOR_CONTROL_TEXT_ACTIVE_HOVERED,
    LDK_UI_COLOR_CONTROL_TEXT_DISABLED,
    LDK_UI_COLOR_CONTROL_BORDER,
    LDK_UI_COLOR_CONTROL_BORDER_HOVERED,
    LDK_UI_COLOR_CONTROL_BORDER_ACTIVE,
    LDK_UI_COLOR_CONTROL_BORDER_ACTIVE_HOVERED,
    LDK_UI_COLOR_CONTROL_BORDER_DISABLED,
    LDK_UI_COLOR_BORDER,
    LDK_UI_COLOR_FOCUS,
    LDK_UI_COLOR_SLIDER_TRACK,
    LDK_UI_COLOR_SLIDER_TRACK_HOVERED,
    LDK_UI_COLOR_SLIDER_TRACK_ACTIVE,
    LDK_UI_COLOR_SLIDER_FILL,
    LDK_UI_COLOR_SLIDER_THUMB,
    LDK_UI_COLOR_SLIDER_THUMB_HOVERED,
    LDK_UI_COLOR_SLIDER_THUMB_ACTIVE,
    LDK_UI_COLOR_TITLE_BAR,
    LDK_UI_COLOR_TITLE_BAR_FOCUSED,
    LDK_UI_COLOR_SCROLLBAR_TRACK,
    LDK_UI_COLOR_SCROLLBAR_THUMB,
    LDK_UI_COLOR_SCROLLBAR_THUMB_HOVERED,
    LDK_UI_COLOR_SCROLLBAR_THUMB_ACTIVE,
    LDK_UI_COLOR_SEPARATOR,
    LDK_UI_COLOR_COUNT,
  } LDKUIColorSlot;

  typedef struct LDKUITheme
  {
    rgba32 colors[LDK_UI_COLOR_COUNT];
    float control_border_size;
    float window_border_size;
    float window_interaction_border_size;
    float slider_track_height;
    float slider_thumb_width;
  } LDKUITheme;

  typedef struct LDKUIVertex
  {
    float x;
    float y;
    float u;
    float v;
    rgba32 color;
  } LDKUIVertex;

  typedef struct LDKUIDrawCmd
  {
    LDKUITextureHandle texture;
    LDKUIRect clip_rect;
    u32 index_offset;
    u32 index_count;
  } LDKUIDrawCmd;

  X_ARRAY_TYPE_NAMED(LDKUIId, ldk_ui_id);
  X_ARRAY_TYPE_NAMED(LDKUIVertex, ldk_ui_vertex);
  X_ARRAY_TYPE_NAMED(uint32_t, ldk_ui_u32);
  X_ARRAY_TYPE_NAMED(LDKUIDrawCmd, ldk_ui_draw_cmd);
  X_ARRAY_TYPE_NAMED(bool, ldk_ui_bool);

  typedef enum LDKUIThemeType
  {
    LDK_UI_THEME_CUSTOM = 0,
    LDK_UI_THEME_DEFAULT_LIGHT = 1,
    LDK_UI_THEME_DEFAULT_DARK = 2,
  } LDKUIThemeType;

  typedef struct LDKUIRenderData
  {
    LDKUIVertex const *vertices;
    u32 vertex_count;
    u32 const *indices;
    u32 index_count;
    LDKUIDrawCmd const *commands;
    uint32_t command_count;
  } LDKUIRenderData;

  typedef struct LDKUIIcon
  {
    uintptr_t texture;
    LDKUIRect uv;
    LDKUISize size;
  } LDKUIIcon;

  typedef enum LDKUILayoutDirection
  {
    LDK_UI_LAYOUT_VERTICAL = 0,
    LDK_UI_LAYOUT_HORIZONTAL = 1,
  } LDKUILayoutDirection;

  typedef enum LDKUIItemType
  {
    LDK_UI_ITEM_WINDOW = 1,
    LDK_UI_ITEM_LABEL = 2,
    LDK_UI_ITEM_BUTTON = 3,
    LDK_UI_ITEM_SLIDER = 4,
    LDK_UI_ITEM_SCROLLVIEW = 5,
    LDK_UI_ITEM_LAYOUT_VERTICAL = 6,
    LDK_UI_ITEM_LAYOUT_HORIZONTAL = 7,
    LDK_UI_ITEM_TEXT_BOX = 8,
    LDK_UI_ITEM_IMAGE = 9,
    LDK_UI_ITEM_ICON_BUTTON = 10,
    LDK_UI_ITEM_PANEL = 11,
    LDK_UI_ITEM_SPACER = 12,
    LDK_UI_ITEM_TOGGLE = 13,
    LDK_UI_ITEM_SEPARATOR = 13,
  } LDKUIItemType;

  typedef enum LDKUISizeMode
  {
    LDK_UI_SIZE_PIXELS = 0,
    LDK_UI_SIZE_PERCENT = 1,
    LDK_UI_SIZE_FILL = 2,
    LDK_UI_SIZE_AUTO = 3,
  } LDKUISizeMode;

  typedef struct LDKUILayoutSize
  {
    LDKUISizeMode mode;
    float value;
  } LDKUILayoutSize;

  typedef enum LDKUIWindowFlags
  {
    LDK_UI_WINDOW_NONE = 0,
    LDK_UI_WINDOW_TITLE_BAR = 1 << 0,
    LDK_UI_WINDOW_DRAGGABLE = 1 << 1,
    LDK_UI_WINDOW_RESIZABLE = 1 << 2,
    LDK_UI_WINDOW_BORDER = 1 << 3,
    LDK_UI_WINDOW_NO_BG = 1 << 4,
    LDK_UI_WINDOW_CLOSE_BUTTON = 1 << 5,
    LDK_UI_WINDOW_NO_PADDING = 1 << 6,
    LDK_UI_WINDOW_TOOL = LDK_UI_WINDOW_CLOSE_BUTTON | LDK_UI_WINDOW_TITLE_BAR |
                         LDK_UI_WINDOW_DRAGGABLE | LDK_UI_WINDOW_RESIZABLE |
                         LDK_UI_WINDOW_BORDER,
  } LDKUIWindowFlags;

  typedef enum LDKUIScrollViewFlags
  {
    LDK_UI_SCROLL_NONE = 0,
    LDK_UI_SCROLL_VERTICAL = 1 << 0,
    LDK_UI_SCROLL_HORIZONTAL = 1 << 1,
    LDK_UI_SCROLL_IF_NEEDED = 1 << 2,
  } LDKUIScrollViewFlags;

  typedef enum LDKTreeNodeFlags
  {
    LDK_UI_TREE_NODE_NONE = 0,
    LDK_UI_TREE_NODE_LEAF = 1 << 0,
    LDK_UI_TREE_NODE_SELECTED = 1 << 1
  } LDKTreeNodeFlags;

  typedef struct LDKUIMark
  {
    LDKUILayout *layout;
    u32 measure_index;
  } LDKUIMark;

  typedef struct LDKUIMeasureEntry
  {
    LDKUILayout *layout;
    LDKUIRect rect;
  } LDKUIMeasureEntry;

  typedef struct LDKUILayoutRequest
  {
    LDKUIItemType type;
    LDKUISize min_size;
    LDKUILayoutSize width;
    LDKUILayoutSize height;
    float weight;
    bool has_height;
    bool has_width;
    bool hit_test;
  } LDKUILayoutRequest;

  typedef struct LDKUILayoutItem
  {
    LDKUIId layout_id;
    u32 item_index;
    LDKUILayoutRequest request;
    LDKUIRect fallback_rect;
  } LDKUILayoutItem;

  typedef struct LDKUILayoutItemCache
  {
    LDKUIId layout_id;
    u32 item_index;
    LDKUIRect rect;
    u32 last_frame_touched;
  } LDKUILayoutItemCache;

  struct LDKUIScrollViewCache
  {
    LDKUIId id;
    float content_w;
    float content_h;
    float scroll_x;
    float scroll_y;
    float returned_scroll_x;
    float returned_scroll_y;
    u32 last_frame_touched;
    bool has_scroll;
  };

  struct LDKUIScrollViewStackEntry
  {
    LDKUIId id;
    LDKUILayout *layout;
    LDKUIMark mark;
    LDKUIRect rect;
    LDKUIRect view_rect;
    LDKUIRect content_rect;
    LDKUIRect previous_clip_rect;
    LDKUIPoint scroll;
    u32 flags;
  };

  struct LDKUILayout
  {
    LDKUIId id;
    LDKUILayoutDirection direction;
    LDKUIRect rect;
    LDKUIRect content_rect;
    LDKUIRect bounding_rect;
    LDKUIPoint cursor;
    float spacing;
    float padding;
    float content_used_right;
    float content_used_bottom;
    u32 item_count;
    bool has_measure_entry;
    u32 measure_entry_index;
    bool has_requested_size_override;
    LDKUISize requested_size_override;
    bool skip_parent_item_min_size_update;
    bool disable_main_axis_expand;
    LDKUILayout *parent;
  };

  struct LDKUIWindow
  {
    LDKUIId id;
    char title[64];
    LDKUIRect rect;
    LDKUIRect title_bar_rect;
    LDKUIRect content_rect;
    u32 flags;
    i32 z_order;
    u32 last_frame_seen;
    LDKUIId focused_id;
    XArray_ldk_ui_vertex *vertices;
    XArray_ldk_ui_u32 *indices;
    XArray_ldk_ui_draw_cmd *commands;
    bool close_requested;
  };

  struct LDKUIWindowStackEntry
  {
    LDKUIWindow *window;
    LDKUIWindow *previous_window;
    LDKUIRect previous_clip_rect;
    u32 layout_stack_count;
  };

  typedef struct LDKUIConfig
  {
    u32 frame_arena_size;
    u32 initial_vertex_capacity;
    u32 initial_index_capacity;
    u32 initial_command_capacity;
    u32 initial_window_capacity;
    u32 initial_id_stack_capacity;
    LDKFontInstance *font;
    void *font_texture_user;
    LDKUIGetFontPageTextureFn get_font_page_texture;
    LDKUIThemeType theme;
    u32 font_size;
  } LDKUIConfig;

  typedef enum LDKUIControlVisualState
  {
    LDK_UI_CONTROL_VISUAL_STATE_IDLE = 0,
    LDK_UI_CONTROL_VISUAL_STATE_HOVERED = 1,
    LDK_UI_CONTROL_VISUAL_STATE_ACTIVE = 2,
    LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED = 3,
    LDK_UI_CONTROL_VISUAL_STATE_DISABLED = 4,
  } LDKUIControlVisualState;

  typedef struct LDKUITextInputState
  {
    u32 codepoints[LDK_UI_INPUT_CODEPOINTS_CAPACITY];
    u32 codepoint_count;
  } LDKUITextInputState;

  typedef enum LDKUITextBoxResult
  {
    LDK_UI_TEXT_BOX_NONE = 0,
    LDK_UI_TEXT_BOX_CHANGED = 1 << 0,
    LDK_UI_TEXT_BOX_COMMITTED = 1 << 1,
    LDK_UI_TEXT_BOX_CANCELED = 1 << 2,
  } LDKUITextBoxResult;

  typedef enum LDKUIHitLayer
  {
    LDK_UI_HIT_LAYER_NORMAL = 0,
    LDK_UI_HIT_LAYER_POPUP = 1,
  } LDKUIHitLayer;

  struct LDKUIHitCandidate
  {
    LDKUIId window_id;
    LDKUIId item_id;
    LDKUIRect rect;
    LDKUIRect clip_rect;
    LDKUIHitLayer layer;
    u32 order;
  };

  X_ARRAY_TYPE_NAMED(LDKUIHitCandidate, ldk_ui_hit_candidate);

  typedef struct LDKUIAreaStackEntry
  {
    bool expanded;
    bool opened_layout;
  } LDKUIAreaStackEntry;

  struct LDKUIPopupStackEntry
  {
    LDKUIId id;
    LDKUIRect rect;
    LDKUIRect previous_clip_rect;
    LDKUILayout *previous_layout;
  };

  struct LDKUIPopupFrameEntry
  {
    LDKUIId id;
    LDKUIRect rect;
  };

  X_ARRAY_TYPE_NAMED(LDKUILayout, ldk_ui_layout);
  X_ARRAY_TYPE_NAMED(LDKUIWindow, ldk_ui_window);
  X_ARRAY_TYPE_NAMED(LDKUIWindowStackEntry, ldk_ui_window_stack_entry);
  X_ARRAY_TYPE_NAMED(LDKUIMeasureEntry, ldk_ui_measure_entry);
  X_ARRAY_TYPE_NAMED(LDKUILayoutItem, ldk_ui_layout_item);
  X_ARRAY_TYPE_NAMED(LDKUILayoutItemCache, ldk_ui_layout_item_cache);
  X_ARRAY_TYPE_NAMED(LDKUIScrollViewStackEntry, ldk_ui_scrollview_stack_entry);
  X_ARRAY_TYPE_NAMED(LDKUIScrollViewCache, ldk_ui_scrollview_cache);
  X_ARRAY_TYPE_NAMED(LDKUIAreaStackEntry, ldk_ui_area_stack_entry);
  X_ARRAY_TYPE_NAMED(LDKUIPopupStackEntry, ldk_ui_popup_stack_entry);
  X_ARRAY_TYPE_NAMED(LDKUIPopupFrameEntry, ldk_ui_popup_frame_entry);

  struct LDKUIContext
  {
    XArena *frame_arena;
    LDKFontInstance *font;
    void *font_file;
    void *font_texture_user;
    LDKUIGetFontPageTextureFn get_font_page_texture;

    LDKMouseState const *mouse;
    LDKKeyboardState const *keyboard;
    LDKUITextInputState const *input_text;

    XArray_ldk_ui_id *id_stack;
    XArray_ldk_ui_vertex *vertices;
    XArray_ldk_ui_u32 *indices;
    XArray_ldk_ui_draw_cmd *commands;
    XArray_ldk_ui_vertex *popup_vertices;
    XArray_ldk_ui_u32 *popup_indices;
    XArray_ldk_ui_draw_cmd *popup_commands;
    XArray_ldk_ui_bool *disabled_stack;
    XArray_ldk_ui_hit_candidate *hit_candidates;

    XArray_ldk_ui_layout *layout_stack;
    u32 layout_frame_index;
    LDKUILayout *current_layout;

    XArray_ldk_ui_window *windows;
    u32 window_frame_index;
    LDKUIWindow *current_window;
    XArray_ldk_ui_window_stack_entry *window_stack;

    XArray_ldk_ui_measure_entry *measure_entries;
    u32 last_measure_entry_index;

    XArray_ldk_ui_layout_item *layout_items;
    XArray_ldk_ui_layout_item_cache *layout_item_cache;

    XArray_ldk_ui_scrollview_stack_entry *scrollview_stack;
    XArray_ldk_ui_scrollview_cache *scrollview_cache;

    XArray_ldk_ui_area_stack_entry *area_stack;
    XArray_ldk_ui_id *open_popups;
    XArray_ldk_ui_popup_stack_entry *popup_stack;
    XArray_ldk_ui_popup_frame_entry *popup_frame_entries;

    LDKUILayoutSize next_width;
    LDKUILayoutSize next_height;
    LDKUISize next_min_size;
    float next_weight;
    bool has_next_width;
    bool has_next_height;
    bool has_next_min_size;
    bool has_next_weight;
    u32 root_item_count;

    LDKUITheme theme;
    LDKUIRenderData render_data;
    LDKUIRect viewport;
    LDKUIRect clip_rect;
    LDKUIRect last_rect;
    LDKUIRect last_bounding_rect;

    LDKUIId hot_id;
    LDKUIId next_hot_id;
    LDKUIId active_id;
    LDKUIId focused_id;
    LDKUIId last_id;
    LDKUIId text_box_id;

    LDKUIId hovered_window_id;
    LDKUIId focused_window_id;
    LDKUIId active_window_id;
    LDKUIId dragging_window_id;
    LDKUIId resizing_window_id;

    u32 text_cursor;
    u32 text_select_start;
    u32 text_select_end;

    u32 hit_order;
    u32 frame_index;
    u32 resizing_window_edges;
    i32 next_z_order;

    bool next_disabled;
    bool last_window_close_requested;

    float delta_time;
    float drag_x;
    float drag_y;
    float resize_start_cursor_x;
    float resize_start_cursor_y;
    LDKUIRect resize_start_rect;
    float scrollbar_drag_offset_x;
    float scrollbar_drag_offset_y;
    LDKCursorType cursor_type;
  };

  LDK_API bool ldk_ui_initialize(LDKUIContext *ctx, LDKUIConfig const *config);
  LDK_API void ldk_ui_terminate(LDKUIContext *ctx);

  LDK_API void ldk_ui_set_theme(
      LDKUIContext *ctx, LDKUIThemeType type, LDKUITheme *custom);

  LDK_API void ldk_ui_begin_frame(LDKUIContext *ctx, float delta,
      LDKMouseState const *mouse, LDKKeyboardState const *keyboard,
      LDKUITextInputState const *text_input, LDKUIRect viewport);
  LDK_API void ldk_ui_end_frame(LDKUIContext *ctx);
  LDK_API LDKUIRenderData const *ldk_ui_get_render_data(
      LDKUIContext const *ctx);

  LDK_API void ldk_ui_push_id_u32(LDKUIContext *ctx, uint32_t value);
  LDK_API void ldk_ui_push_id_ptr(LDKUIContext *ctx, void const *value);
  LDK_API void ldk_ui_push_id_cstr(LDKUIContext *ctx, char const *value);
  LDK_API void ldk_ui_pop_id(LDKUIContext *ctx);

  LDK_API void ldk_ui_set_next_disabled(LDKUIContext *ctx, bool disabled);
  LDK_API void ldk_ui_begin_disabled(LDKUIContext *ctx, bool disabled);
  LDK_API void ldk_ui_end_disabled(LDKUIContext *ctx);

  LDK_API LDKUIRect ldk_ui_rect(float x, float y, float w, float h);

  //----------------------------------------------------------
  // Windows
  //----------------------------------------------------------
  LDK_API LDKUIRect ldk_ui_begin_window_fixed(
      LDKUIContext *ctx, char const *title, LDKUIRect rect, u32 flags);
  LDK_API LDKUIRect ldk_ui_begin_window(
      LDKUIContext *ctx, char const *title, LDKUIRect rect, u32 flags);
  LDK_API bool ldk_ui_begin_window_open(LDKUIContext *ctx, char const *title,
      LDKUIRect *rect, bool *open, u32 flags);
  LDK_API bool ldk_ui_window_close_requested(LDKUIContext *ctx);
  LDK_API void ldk_ui_end_window(LDKUIContext *ctx);

  //----------------------------------------------------------
  // Layout size hints
  //----------------------------------------------------------
  LDK_API LDKUILayoutSize ldk_ui_px(float value);
  LDK_API LDKUILayoutSize ldk_ui_percent(float value);
  LDK_API LDKUILayoutSize ldk_ui_fill(void);

  LDK_API void ldk_ui_set_next_width(LDKUIContext *ctx, LDKUILayoutSize width);
  LDK_API void ldk_ui_set_next_height(
      LDKUIContext *ctx, LDKUILayoutSize height);
  LDK_API void ldk_ui_set_next_size(
      LDKUIContext *ctx, LDKUILayoutSize width, LDKUILayoutSize height);
  LDK_API void ldk_ui_set_next_min_size(
      LDKUIContext *ctx, float width, float height);
  LDK_API void ldk_ui_set_next_weight(LDKUIContext *ctx, float weight);

  LDK_API LDKUIRect ldk_ui_last_rect(LDKUIContext *ctx);
  LDK_API LDKUIRect ldk_ui_last_bounding_rect(LDKUIContext *ctx);
  LDK_API LDKUIMark ldk_ui_mark(LDKUIContext *ctx);
  LDK_API LDKUIRect ldk_ui_measure_from(LDKUIContext *ctx, LDKUIMark mark);

  //----------------------------------------------------------
  // Layout
  //----------------------------------------------------------
  LDK_API void ldk_ui_begin_vertical(LDKUIContext *ctx);
  LDK_API void ldk_ui_begin_horizontal(LDKUIContext *ctx);
  LDK_API void ldk_ui_end(LDKUIContext *ctx);
  LDK_API void ldk_ui_end_vertical(LDKUIContext *ctx);
  LDK_API void ldk_ui_end_horizontal(LDKUIContext *ctx);

  //----------------------------------------------------------
  // Scroll views
  //----------------------------------------------------------
  LDK_API LDKUIPoint ldk_ui_begin_scrollview(
      LDKUIContext *ctx, LDKUIPoint scroll, u32 flags);
  LDK_API void ldk_ui_end_scrollview(LDKUIContext *ctx);

  //----------------------------------------------------------
  // Areas
  //----------------------------------------------------------
  LDK_API bool ldk_ui_begin_area(
      LDKUIContext *ctx, char const *title, bool expanded);
  LDK_API void ldk_ui_end_area(LDKUIContext *ctx);

  //----------------------------------------------------------
  // Popups
  //----------------------------------------------------------
  LDK_API void ldk_ui_open_popup(LDKUIContext *ctx, LDKUIId id);
  LDK_API void ldk_ui_close_popup(LDKUIContext *ctx, LDKUIId id);
  LDK_API void ldk_ui_close_current_popup(LDKUIContext *ctx);
  LDK_API void ldk_ui_close_all_popups(LDKUIContext *ctx);
  LDK_API bool ldk_ui_popup_is_open(LDKUIContext *ctx, LDKUIId id);
  LDK_API bool ldk_ui_begin_popup(
      LDKUIContext *ctx, LDKUIId id, LDKUIRect rect);
  LDK_API void ldk_ui_end_popup(LDKUIContext *ctx);

  //----------------------------------------------------------
  // Layout widget wrappers
  //----------------------------------------------------------
  LDK_API void ldk_ui_image(LDKUIContext *ctx, LDKUITextureHandle texture,
      LDKUIRect uv, LDKUISize size);
  LDK_API void ldk_ui_icon(LDKUIContext *ctx, LDKUIIcon icon);
  LDK_API bool ldk_ui_icon_button(LDKUIContext *ctx, LDKUIIcon icon);
  LDK_API void ldk_ui_label(LDKUIContext *ctx, char const *text);
  LDK_API bool ldk_ui_button(LDKUIContext *ctx, char const *text);
  LDK_API bool ldk_ui_toggle(LDKUIContext *ctx, bool value);
  LDK_API bool ldk_ui_button_flat(LDKUIContext *ctx, char const *text);
  LDK_API float ldk_ui_slider(
      LDKUIContext *ctx, float value, float min_value, float max_value);
  LDK_API bool ldk_ui_tree_node(LDKUIContext *ctx, char const *title,
      bool expanded, u32 depth, u32 flags);
  LDK_API u32 ldk_ui_text_box(LDKUIContext *ctx, char *buffer, u32 buffer_size);
  LDK_API void ldk_ui_horizontal_line(LDKUIContext *ctx);
  LDK_API void ldk_ui_spacer(LDKUIContext *ctx);

  //----------------------------------------------------------
  // Base widgets
  //----------------------------------------------------------
  LDK_API void ldk_ui_widget_panel(
      LDKUIContext *ctx, LDKUIId id, LDKUIRect rect);
  LDK_API void ldk_ui_widget_label(
      LDKUIContext *ctx, LDKUIId id, char const *text, LDKUIRect rect);
  LDK_API void ldk_ui_widget_image(LDKUIContext *ctx, LDKUIId id,
      LDKUITextureHandle texture, LDKUIRect uv, LDKUIRect rect);
  LDK_API bool ldk_ui_widget_button(
      LDKUIContext *ctx, LDKUIId id, char const *text, LDKUIRect rect);
  LDK_API bool ldk_ui_widget_button_flat(
      LDKUIContext *ctx, LDKUIId id, char const *text, LDKUIRect rect);
  LDK_API bool ldk_ui_widget_toggle(
      LDKUIContext *ctx, LDKUIId id, bool value, LDKUIRect rect);
  LDK_API float ldk_ui_widget_slider(LDKUIContext *ctx, LDKUIId id, float value,
      float min_value, float max_value, LDKUIRect rect);
  LDK_API float ldk_ui_widget_scrollbar_vertical(LDKUIContext *ctx, LDKUIId id,
      float scroll, float visible_size, float content_size, LDKUIRect rect);
  LDK_API float ldk_ui_widget_scrollbar_horizontal(LDKUIContext *ctx,
      LDKUIId id, float scroll, float visible_size, float content_size,
      LDKUIRect rect);
  LDK_API u32 ldk_ui_widget_text_box(LDKUIContext *ctx, LDKUIId id,
      char *buffer, u32 buffer_size, LDKUIRect rect);

#ifdef __cplusplus
}
#endif

#endif
