/**
 * # Immediate-mode user interface
 *
 * Build UI layouts, windows, widgets, popups, and render data.
 */

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
#include <stdx/stdx_hashtable.h>

#include <stdbool.h>
#include <stdint.h>

#ifndef LDK_UI_INPUT_CODEPOINTS_CAPACITY
/** @group modules.ui.controls */
#define LDK_UI_INPUT_CODEPOINTS_CAPACITY 16
#endif

#ifndef LDK_UI_DEFAULT_CONTROL_HEIGHT
/** @group modules.ui.layout.sizing */
#define LDK_UI_DEFAULT_CONTROL_HEIGHT 22.0f
#endif

#ifndef LDK_UI_DEFAULT_CONTROL_WIDTH
/** @group modules.ui.layout.sizing */
#define LDK_UI_DEFAULT_CONTROL_WIDTH 16.0f
#endif

#ifndef LDK_UI_LAYOUT_STACK_CAPACITY
/** @group modules.ui.lifecycle */
#define LDK_UI_LAYOUT_STACK_CAPACITY 64
#endif

#ifndef LDK_UI_LAYOUT_ITEM_CAPACITY
/** @group modules.ui.lifecycle */
#define LDK_UI_LAYOUT_ITEM_CAPACITY 512
#endif

#ifndef LDK_UI_LAYOUT_ITEM_CACHE_CAPACITY
/** @group modules.ui.lifecycle */
#define LDK_UI_LAYOUT_ITEM_CACHE_CAPACITY 64
#endif

#ifndef LDK_UI_MEASURE_ENTRY_CAPACITY
/** @group modules.ui.layout.measurement */
#define LDK_UI_MEASURE_ENTRY_CAPACITY 64
#endif

#ifndef LDK_UI_WINDOW_CAPACITY
/** @group modules.ui.windows */
#define LDK_UI_WINDOW_CAPACITY 16
#endif

#ifndef LDK_UI_WINDOW_STACK_CAPACITY
/** @group modules.ui.windows */
#define LDK_UI_WINDOW_STACK_CAPACITY 16
#endif

#ifndef LDK_UI_SCROLLVIEW_STACK_CAPACITY
/** @group modules.ui.scrolling */
#define LDK_UI_SCROLLVIEW_STACK_CAPACITY 16
#endif

#ifndef LDK_UI_SCROLLVIEW_CACHE_CAPACITY
/** @group modules.ui.scrolling */
#define LDK_UI_SCROLLVIEW_CACHE_CAPACITY 16
#endif

#ifndef LDK_UI_SCROLLBAR_SIZE
/** @group modules.ui.layout.sizing */
#define LDK_UI_SCROLLBAR_SIZE 12.0f
#endif

#ifndef LDK_UI_AREA_STACK_CAPACITY
/** @group modules.ui.areas */
#define LDK_UI_AREA_STACK_CAPACITY 64
#endif

#ifndef LDK_UI_POPUP_STACK_CAPACITY
/** @group modules.ui.popups */
#define LDK_UI_POPUP_STACK_CAPACITY 16
#endif

#ifndef LDK_UI_MIN_WINDOW_WIDTH
/** @group modules.ui.windows */
#define LDK_UI_MIN_WINDOW_WIDTH 64.0f
#endif

#ifndef LDK_UI_MIN_WINDOW_HEIGHT
/** @group modules.ui.windows */
#define LDK_UI_MIN_WINDOW_HEIGHT 1
#endif

#ifndef LDK_UI_TITLE_BAR_HEIGHT
/** @group modules.ui.layout.sizing */
#define LDK_UI_TITLE_BAR_HEIGHT 24.0f
#endif

#ifndef LDK_UI_DEFAULT_SPACING
/** @group modules.ui.lifecycle */
#define LDK_UI_DEFAULT_SPACING 4.0f
#endif

#ifndef LDK_UI_DEFAULT_PADDING
/** @group modules.ui.layout.spacing */
#define LDK_UI_DEFAULT_PADDING 4.0f
#endif

#ifndef LDK_UI_TREE_NODE_INDENT_WIDTH
/** @group modules.ui.layout.sizing */
#define LDK_UI_TREE_NODE_INDENT_WIDTH 8.0f
#endif

#ifndef LDK_UI_TREE_NODE_CHEVRON_WIDTH
/** @group modules.ui.layout.sizing */
#define LDK_UI_TREE_NODE_CHEVRON_WIDTH 10.0f
#endif

#ifndef LDK_UI_TAB_BAR_TAB_HEIGHT
/** @group modules.ui.layout.sizing */
#define LDK_UI_TAB_BAR_TAB_HEIGHT 34.0f
#endif

#ifndef LDK_UI_TAB_BAR_HEIGHT
/** @group modules.ui.layout.sizing */
#define LDK_UI_TAB_BAR_HEIGHT 34.0f
#endif

#ifndef LDK_UI_TAB_BAR_TAB_PADDING
/** @group modules.ui.layout.spacing */
#define LDK_UI_TAB_BAR_TAB_PADDING 14.0f
#endif

#ifndef LDK_UI_AREA_HEADER_HEIGHT
/** @group modules.ui.layout.sizing */
#define LDK_UI_AREA_HEADER_HEIGHT 34.0f
#endif

#ifndef LDK_UI_AREA_PADDING
/** @group modules.ui.layout.spacing */
#define LDK_UI_AREA_PADDING 8.0f
#endif

#ifndef LDK_UI_TAB_BAR_MENU_BUTTON_WIDTH
/** @group modules.ui.layout.sizing */
#define LDK_UI_TAB_BAR_MENU_BUTTON_WIDTH 24.0f
#endif

#ifndef LDK_UI_TAB_BAR_SPACING
/** @group modules.ui.tabs */
#define LDK_UI_TAB_BAR_SPACING 2.0f
#endif

#ifndef LDK_UI_TAB_BAR_LINE_THICKNESS
/** @group modules.ui.tabs */
#define LDK_UI_TAB_BAR_LINE_THICKNESS 1.2f
#endif

  /** @group modules.ui.lifecycle */
  typedef uint32_t LDKUIId;
  /** @group modules.ui.lifecycle */
  typedef uintptr_t LDKUITextureHandle;
  /**
   * @brief Callback resolving the texture for a font atlas page.
   * @param user Opaque caller-supplied data passed to the callback.
   * @param font Font or font instance used by the operation.
   * @param page_index Index of the requested font or asset page.
   * @return Handle to the requested object or resource.
   * @group modules.ui.lifecycle
   */
  typedef LDKUITextureHandle (*LDKUIGetFontPageTextureFn)(
      void *user, LDKFontInstance *font, u32 page_index);
  /** @group modules.ui.lifecycle */
  typedef u32 LDKUIColor;
  /** @group modules.ui.lifecycle */
  typedef LDKPointf LDKUIPoint;
  /** @group modules.ui.layout.sizing */
  typedef LDKSizef LDKUISize;
  /** @group modules.ui.layout.geometry */
  typedef LDKRectf LDKUIRect;

  /** @group modules.ui.lifecycle */
  typedef struct LDKUILayout LDKUILayout;
  /** @group modules.ui.windows */
  typedef struct LDKUIWindow LDKUIWindow;
  /** @group modules.ui.windows */
  typedef struct LDKUIWindowStackEntry LDKUIWindowStackEntry;
  /** @group modules.ui.scrolling */
  typedef struct LDKUIScrollViewCache LDKUIScrollViewCache;
  /** @group modules.ui.scrolling */
  typedef struct LDKUIScrollViewStackEntry LDKUIScrollViewStackEntry;
  /** @group modules.ui.areas */
  typedef struct LDKUIAreaStackEntry LDKUIAreaStackEntry;
  /** @group modules.ui.popups */
  typedef struct LDKUIPopupStackEntry LDKUIPopupStackEntry;
  /** @group modules.ui.popups */
  typedef struct LDKUIPopupFrameEntry LDKUIPopupFrameEntry;
  /** @group modules.ui.popups */
  typedef struct LDKUIPopupCache LDKUIPopupCache;
  /** @group modules.ui.lifecycle */
  typedef struct LDKUIHitCandidate LDKUIHitCandidate;
  /** @group modules.ui.icons */
  typedef struct LDKUIContext LDKUIContext;

  /** @group modules.ui.lifecycle */
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
    LDK_UI_COLOR_TITLE,
    LDK_UI_COLOR_TITLE_BAR,
    LDK_UI_COLOR_TITLE_BAR_FOCUSED,

    // Scroll bar
    LDK_UI_COLOR_SCROLLBAR_TRACK,
    LDK_UI_COLOR_SCROLLBAR_THUMB,
    LDK_UI_COLOR_SCROLLBAR_THUMB_HOVERED,
    LDK_UI_COLOR_SCROLLBAR_THUMB_ACTIVE,

    // Tab bar
    LDK_UI_COLOR_TAB_BAR_BG,
    LDK_UI_COLOR_TAB_BAR_SEPARATOR,
    LDK_UI_COLOR_TAB_BG,
    LDK_UI_COLOR_TAB_BG_HOVERED,
    LDK_UI_COLOR_TAB_TEXT,
    LDK_UI_COLOR_TAB_TEXT_HOVERED,
    LDK_UI_COLOR_TAB_BORDER,
    LDK_UI_COLOR_TAB_BORDER_HOVERED,
    LDK_UI_COLOR_TAB_ACTIVE_BG,
    LDK_UI_COLOR_TAB_ACTIVE_TEXT,
    LDK_UI_COLOR_TAB_ACTIVE_BORDER,

    LDK_UI_COLOR_SEPARATOR,
    LDK_UI_COLOR_INPUT_BORDER,
    LDK_UI_COLOR_INPUT_BG,
    LDK_UI_COLOR_INPUT_BG_HOVERED,
    LDK_UI_COLOR_INPUT_BG_ACTIVE,
    LDK_UI_COLOR_INPUT_BG_ACTIVE_HOVERED,
    LDK_UI_COLOR_COUNT,
  } LDKUIColorSlot;

  /** @group modules.ui.icons */
  typedef struct LDKUIIcon
  {
    uintptr_t texture;
    LDKUIRect uv;
    LDKUISize size;
    rgba32 color;
  } LDKUIIcon;

  /** @group modules.ui.theme */
  typedef enum LDKUIThemeIconSlot
  {
    LDK_UI_THEME_ICON_TREE_NODE_COLLAPSED,
    LDK_UI_THEME_ICON_TREE_NODE_EXPANDED,
    LDK_UI_THEME_ICON_TOGGLE_UNCHECKED,
    LDK_UI_THEME_ICON_TOGGLE_CHECKED,
    LDK_UI_THEME_ICON_MORE_HORIZ,
    LDK_UI_THEME_ICON_MORE_VERT,
    LDK_UI_THEME_ICON_EJECT,
    LDK_UI_THEME_ICON_COUNT,
  } LDKUIThemeIconSlot;

  /** @group modules.ui.theme */
  typedef struct LDKUITheme
  {
    rgba32 colors[LDK_UI_COLOR_COUNT];
    LDKUIIcon icons[LDK_UI_THEME_ICON_COUNT];
    float control_border_size;
    float input_border_size;
    float window_border_size;
    float window_interaction_border_size;
    float slider_track_height;
    float slider_thumb_width;
    bool text_cursor_blink;
    float text_cursor_blink_interval;
    float text_cursor_width;
    float text_cursor_padding_y;
    /** Uniform content inset for windows and panels, in pixels. */
    float panel_padding;
  } LDKUITheme;

  /** @group modules.ui.lifecycle */
  typedef struct LDKUIVertex
  {
    float x;
    float y;
    float u;
    float v;
    rgba32 color;
  } LDKUIVertex;

  /** @group modules.ui.lifecycle */
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

  /** @group modules.ui.theme */
  typedef enum LDKUIThemeType
  {
    LDK_UI_THEME_CUSTOM = 0,
    LDK_UI_THEME_DEFAULT_LIGHT = 1,
    LDK_UI_THEME_DEFAULT_DARK = 2,
  } LDKUIThemeType;

  /** @group modules.ui.lifecycle */
  typedef struct LDKUIRenderData
  {
    LDKUIVertex const *vertices;
    u32 vertex_count;
    u32 const *indices;
    u32 index_count;
    LDKUIDrawCmd const *commands;
    uint32_t command_count;
    /** Logical viewport used to generate the vertices and clip rectangles. */
    LDKUISize viewport_size;
  } LDKUIRenderData;

  /** @group modules.ui.layout.geometry */
  typedef enum LDKUILayoutDirection
  {
    LDK_UI_LAYOUT_VERTICAL = 0,
    LDK_UI_LAYOUT_HORIZONTAL = 1,
  } LDKUILayoutDirection;

  /** @group modules.ui.lifecycle */
  typedef enum LDKUIItemType
  {
    LDK_UI_ITEM_WINDOW = 1,
    LDK_UI_ITEM_LABEL = 2,
    LDK_UI_ITEM_BUTTON = 3,
    LDK_UI_ITEM_SLIDER = 4,
    LDK_UI_ITEM_SCROLLVIEW = 5,
    LDK_UI_ITEM_LAYOUT_VERTICAL = 6,
    LDK_UI_ITEM_LAYOUT_HORIZONTAL = 7,
    LDK_UI_ITEM_INPUT_BOX = 8,
    LDK_UI_ITEM_IMAGE = 9,
    LDK_UI_ITEM_ICON_BUTTON = 10,
    LDK_UI_ITEM_PANEL = 11,
    LDK_UI_ITEM_SPACER = 12,
    LDK_UI_ITEM_TOGGLE = 13,
    LDK_UI_ITEM_SEPARATOR = 14,
    LDK_UI_ITEM_ICON_LABEL = 15,
    LDK_UI_ITEM_COMBO_BOX = 16,
    LDK_UI_ITEM_TAB_BAR = 17,
    LDK_UI_ITEM_RESIZE_HANDLE = 18,
    LDK_UI_ITEM_SLIDER_INPUT = 19,
  } LDKUIItemType;

  /** @group modules.ui.layout.sizing */
  typedef enum LDKUISizeMode
  {
    LDK_UI_SIZE_PIXELS = 0,
    LDK_UI_SIZE_PERCENT = 1,
    LDK_UI_SIZE_FILL = 2,
    LDK_UI_SIZE_AUTO = 3,
  } LDKUISizeMode;

  /** @group modules.ui.layout.sizing */
  typedef struct LDKUILayoutSize
  {
    LDKUISizeMode mode;
    float value;
  } LDKUILayoutSize;

  /** @group modules.ui.windows */
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
    LDK_UI_WINDOW_CACHEABLE = 1 << 7,
    LDK_UI_WINDOW_TOOL = LDK_UI_WINDOW_CLOSE_BUTTON | LDK_UI_WINDOW_TITLE_BAR |
                         LDK_UI_WINDOW_DRAGGABLE | LDK_UI_WINDOW_RESIZABLE |
                         LDK_UI_WINDOW_BORDER,
  } LDKUIWindowFlags;

  /** @group modules.ui.scrolling */
  typedef enum LDKUIScrollViewFlags
  {
    LDK_UI_SCROLL_NONE = 0,
    LDK_UI_SCROLL_VERTICAL = 1 << 0,
    LDK_UI_SCROLL_HORIZONTAL = 1 << 1,
    LDK_UI_SCROLL_IF_NEEDED = 1 << 2,
  } LDKUIScrollViewFlags;

  /** @group modules.ui.trees */
  typedef enum LDKTreeNodeFlags
  {
    LDK_UI_TREE_NODE_NONE = 0,
    LDK_UI_TREE_NODE_LEAF = 1 << 0,
    LDK_UI_TREE_NODE_SELECTED = 1 << 1
  } LDKTreeNodeFlags;

  /** @group modules.ui.trees */
  typedef enum LDKTreeNodeResult
  {
    LDK_UI_TREE_NODE_RESULT_NONE = 0,
    LDK_UI_TREE_NODE_RESULT_CLICKED = 1 << 0,
    LDK_UI_TREE_NODE_RESULT_TOGGLED = 1 << 1,
  } LDKTreeNodeResult;

  /** @group modules.ui.layout.measurement */
  typedef struct LDKUIMark
  {
    LDKUILayout *layout;
    u32 measure_index;
  } LDKUIMark;

  /** @group modules.ui.layout.measurement */
  typedef struct LDKUIMeasureEntry
  {
    LDKUILayout *layout;
    LDKUIRect rect;
  } LDKUIMeasureEntry;

  /** @group modules.ui.lifecycle */
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

  /** @group modules.ui.lifecycle */
  typedef struct LDKUILayoutItem
  {
    LDKUIId layout_id;
    u32 item_index;
    u32 next_layout_item_array_index;
    u32 cache_array_index;
    LDKUILayoutRequest request;
    LDKUIRect fallback_rect;
  } LDKUILayoutItem;

  /** @group modules.ui.lifecycle */
  typedef struct LDKUILayoutItemCache
  {
    LDKUIId layout_id;
    LDKUIId owner_window_id;
    u32 item_index;
    LDKUIRect rect;
    u32 last_frame_touched;
  } LDKUILayoutItemCache;

  /** @group modules.ui.lifecycle */
  struct LDKUIScrollViewCache
  {
    LDKUIId id;
    LDKUIId owner_window_id;
    float content_w;
    float content_h;
    float scroll_x;
    float scroll_y;
    float returned_scroll_x;
    float returned_scroll_y;
    u32 last_frame_touched;
    bool has_scroll;
  };

  /** @group modules.ui.lifecycle */
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

  /** @group modules.ui.lifecycle */
  struct LDKUILayout
  {
    LDKUIId id;
    LDKUILayoutDirection direction;
    LDKUIRect rect;
    LDKUIRect content_rect;
    LDKUIRect bounding_rect;
    LDKUIRect unpadded_content_rect;
    LDKUIPoint cursor;
    float spacing;
    float padding;
    float content_used_right;
    float content_used_bottom;
    u32 item_count;
    u32 first_item_array_index;
    u32 last_item_array_index;
    u32 parent_item_array_index;
    bool has_measure_entry;
    u32 measure_entry_index;
    bool has_requested_size_override;
    LDKUISize requested_size_override;
    bool skip_parent_item_min_size_update;
    bool disable_main_axis_expand;
    LDKUILayout *parent;
  };

  /** @group modules.ui.lifecycle */
  struct LDKUIWindow
  {
    LDKUIId id;
    char title[64];
    LDKUIRect rect;
    LDKUIRect title_bar_rect;
    LDKUIRect content_rect;
    LDKUIRect cache_parent_clip;
    u32 flags;
    i32 z_order;
    u32 last_frame_seen;
    LDKUIId focused_id;
    XArray_ldk_ui_vertex *vertices;
    XArray_ldk_ui_u32 *indices;
    XArray_ldk_ui_draw_cmd *commands;
    LDKFontInstance *cache_font;
    u8 cache_idle_frames;
    bool close_requested;
    bool cache_valid;
    bool cache_dirty;
    bool cache_reused;
  };

  /** @group modules.ui.windows */
  struct LDKUIWindowStackEntry
  {
    LDKUIWindow *window;
    LDKUIWindow *previous_window;
    LDKUIRect previous_clip_rect;
    u32 layout_stack_count;
  };

  /** @group modules.ui.icons */
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

  /** @group modules.ui.icons */
  typedef enum LDKUIControlVisualState
  {
    LDK_UI_CONTROL_VISUAL_STATE_IDLE = 0,
    LDK_UI_CONTROL_VISUAL_STATE_HOVERED = 1,
    LDK_UI_CONTROL_VISUAL_STATE_ACTIVE = 2,
    LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED = 3,
    LDK_UI_CONTROL_VISUAL_STATE_DISABLED = 4,
  } LDKUIControlVisualState;

  /** @group modules.ui.controls */
  typedef struct LDKUITextInputState
  {
    u32 codepoints[LDK_UI_INPUT_CODEPOINTS_CAPACITY];
    u32 codepoint_count;
  } LDKUITextInputState;
  /** @group modules.ui.controls */
  typedef enum LDKUIInputBoxResult
  {
    LDK_UI_INPUT_BOX_NONE = 0,
    LDK_UI_INPUT_BOX_CHANGED = 1 << 0,
    LDK_UI_INPUT_BOX_COMMITTED = 1 << 1,
    LDK_UI_INPUT_BOX_CANCELED = 1 << 2,
  } LDKUIInputBoxResult;

  /** @group modules.ui.controls */
  typedef struct LDKUISliderInputState
  {
    LDKUIId id;
    char buffer[64];
    float initial_value;
    float value;
  } LDKUISliderInputState;

  /** @group modules.ui.lifecycle */
  typedef enum LDKUIHitLayer
  {
    LDK_UI_HIT_LAYER_NORMAL = 0,
    LDK_UI_HIT_LAYER_POPUP = 1,
  } LDKUIHitLayer;

  /** @group modules.ui.lifecycle */
  struct LDKUIHitCandidate
  {
    LDKUIId window_id;
    LDKUIId item_id;
    LDKUIRect rect;
    LDKUIRect clip_rect;
    LDKUIHitLayer layer;
    u32 order;
  };

  /** @group modules.ui.state */
  typedef struct LDKUITabFocusEntry
  {
    LDKUIId window_id;
    LDKUIId item_id;
  } LDKUITabFocusEntry;

  X_ARRAY_TYPE_NAMED(LDKUIHitCandidate, ldk_ui_hit_candidate);
  X_ARRAY_TYPE_NAMED(LDKUITabFocusEntry, ldk_ui_tab_focus_entry);

  /** @group modules.ui.areas */
  typedef struct LDKUIAreaStackEntry
  {
    bool expanded;
    bool opened_layout;
    LDKUIRect header_rect;
    XArray_ldk_ui_vertex *background_vertices;
    u32 background_vertex_index;
    bool has_background;
  } LDKUIAreaStackEntry;

  /** @group modules.ui.lifecycle */
  struct LDKUIPopupStackEntry
  {
    LDKUIId id;
    LDKUIRect rect;
    LDKUIRect previous_clip_rect;
    LDKUILayout *previous_layout;
    u32 measure_entry_index;
  };

  /** @group modules.ui.lifecycle */
  struct LDKUIPopupFrameEntry
  {
    LDKUIId id;
    LDKUIRect rect;
  };

  /** @group modules.ui.lifecycle */
  struct LDKUIPopupCache
  {
    LDKUIId id;
    LDKUIPoint position;
    LDKUIRect rect;
    u32 last_frame_touched;
    bool has_rect;
  };

  /** @group modules.ui.tabs */
  typedef struct LDKUITabBarItem
  {
    LDKUIId id;
    LDKUIIcon icon;
    char const *label;
  } LDKUITabBarItem;

  /** @group modules.ui.tabs */
  typedef struct LDKUITabBarResult
  {
    u32 active_index;
    u32 pressed_index;
    bool changed;
  } LDKUITabBarResult;

  X_ARRAY_TYPE_NAMED(LDKUILayout, ldk_ui_layout);
  X_ARRAY_TYPE_NAMED(LDKUIWindow, ldk_ui_window);
  X_ARRAY_TYPE_NAMED(LDKUIWindowStackEntry, ldk_ui_window_stack_entry);
  X_ARRAY_TYPE_NAMED(LDKUIMeasureEntry, ldk_ui_measure_entry);
  X_ARRAY_TYPE_NAMED(LDKUILayoutItem, ldk_ui_layout_item);
  X_ARRAY_TYPE_NAMED(LDKUILayoutItemCache, ldk_ui_layout_item_cache);
  X_HASHTABLE_TYPE_NAMED(u64, u32, ldk_ui_layout_item_cache_index);
  X_ARRAY_TYPE_NAMED(LDKUIScrollViewStackEntry, ldk_ui_scrollview_stack_entry);
  X_ARRAY_TYPE_NAMED(LDKUIScrollViewCache, ldk_ui_scrollview_cache);
  X_ARRAY_TYPE_NAMED(LDKUIAreaStackEntry, ldk_ui_area_stack_entry);
  X_ARRAY_TYPE_NAMED(LDKUIPopupStackEntry, ldk_ui_popup_stack_entry);
  X_ARRAY_TYPE_NAMED(LDKUIPopupFrameEntry, ldk_ui_popup_frame_entry);
  X_ARRAY_TYPE_NAMED(LDKUIPopupCache, ldk_ui_popup_cache);

  /** @group modules.ui.icons */
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
    LDKWindow clipboard_window;

    XArray_ldk_ui_id *id_stack;
    XArray_ldk_ui_vertex *vertices;
    XArray_ldk_ui_u32 *indices;
    XArray_ldk_ui_draw_cmd *commands;
    XArray_ldk_ui_vertex *popup_vertices;
    XArray_ldk_ui_u32 *popup_indices;
    XArray_ldk_ui_draw_cmd *popup_commands;
    XArray_ldk_ui_bool *disabled_stack;
    XArray_ldk_ui_hit_candidate *hit_candidates;
    XArray_ldk_ui_tab_focus_entry *tab_focus_entries;
    XArray_ldk_ui_tab_focus_entry *previous_tab_focus_entries;

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
    XHashtable_ldk_ui_layout_item_cache_index *layout_item_cache_index;

    XArray_ldk_ui_scrollview_stack_entry *scrollview_stack;
    XArray_ldk_ui_scrollview_cache *scrollview_cache;

    XArray_ldk_ui_area_stack_entry *area_stack;
    XArray_ldk_ui_id *open_popups;
    XArray_ldk_ui_popup_stack_entry *popup_stack;
    XArray_ldk_ui_popup_frame_entry *popup_frame_entries;
    XArray_ldk_ui_popup_cache *popup_cache;

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
    LDKUIId input_box_id;
    LDKUIId tab_focus_id;
    LDKUIId tab_focus_window_id;

    LDKUIId hovered_window_id;
    LDKUIId focused_window_id;
    LDKUIId active_window_id;
    LDKUIId dragging_window_id;
    LDKUIId resizing_window_id;

    LDKUISliderInputState slider_input;

    u32 text_cursor;
    u32 text_select_start;
    u32 text_select_end;
    float text_cursor_blink_timer;
    bool text_cursor_blink_visible;

    u32 hit_order;
    u32 profile_widget_box_count;
    u32 profile_fully_clipped_widget_box_count;
    u32 profile_fully_clipped_hit_candidate_count;
    u32 profile_text_measure_count;
    u32 profile_layout_text_measure_count;
    u32 profile_widget_text_measure_count;
    u32 profile_input_text_measure_count;
    u32 profile_input_duplicate_text_measure_count;
    u32 frame_index;
    u32 resizing_window_edges;
    i32 next_z_order;

    bool next_disabled;
    bool next_focus;
    bool last_window_close_requested;

    float delta_time;
    float drag_x;
    float drag_y;
    float resize_start_cursor_x;
    float resize_start_cursor_y;
    LDKUIRect resize_start_rect;
    float scrollbar_drag_offset_x;
    float scrollbar_drag_offset_y;
    float resize_handle_drag_start_cursor;
    float resize_handle_drag_start_value;
    LDKCursorType cursor_type;
  };

  /**
   * @brief Initialize the UI.
   * @param ctx UI context used by this operation.
   * @param config Configuration for this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group modules.ui.lifecycle
   */
  LDK_API bool ldk_ui_initialize(LDKUIContext *ctx, LDKUIConfig const *config);
  /**
   * @brief Shut down the UI.
   * @param ctx UI context used by this operation.
   * @group modules.ui.lifecycle
   */
  LDK_API void ldk_ui_terminate(LDKUIContext *ctx);

  /**
   * @brief Get the UI theme.
   * @param type Type identifier or kind used by the operation.
   * @param theme Theme used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group modules.ui.theme
   */
  LDK_API bool ldk_ui_theme_get(LDKUIThemeType type, LDKUITheme *theme);
  /**
   * @brief Set the UI theme.
   * @param ctx UI context used by this operation.
   * @param theme Theme used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group modules.ui.theme
   */
  LDK_API bool ldk_ui_theme_set(LDKUIContext *ctx, LDKUITheme *theme);

  /**
   * @brief Start a UI frame with the current input snapshots and viewport.
   * @param ctx UI context used by this operation.
   * @param delta Elapsed time since the preceding frame, in seconds.
   * @param mouse Mouse used by this operation.
   * @param keyboard Keyboard used by this operation.
   * @param text_input Text input used by this operation.
   * @param viewport Viewport used by this operation.
   * @group modules.ui.frames
   */
  LDK_API void ldk_ui_begin_frame(LDKUIContext *ctx, float delta,
      LDKMouseState const *mouse, LDKKeyboardState const *keyboard,
      LDKUITextInputState const *text_input, LDKUIRect viewport);
  /**
   * @brief Finish the current UI frame and prepare render data.
   * @param ctx UI context used by this operation.
   * @group modules.ui.frames
   */
  LDK_API void ldk_ui_end_frame(LDKUIContext *ctx);
  /**
   * @brief Access draw data produced by the completed UI frame.
   * @param ctx UI context used by this operation.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group modules.ui.frames
   */
  LDK_API LDKUIRenderData const *ldk_ui_get_render_data(
      LDKUIContext const *ctx);
  /**
   * @brief Associate a native window with UI clipboard operations.
   * @param ctx UI context used by this operation.
   * @param window Native window handle.
   * @group modules.ui.clipboard
   */
  LDK_API void ldk_ui_clipboard_window_set(
      LDKUIContext *ctx, LDKWindow window);

  /**
   * @brief Push an integer onto the hierarchical UI ID stack.
   * @param ctx UI context used by this operation.
   * @param value Input value or current widget value.
   * @group modules.ui.ids
   */
  LDK_API void ldk_ui_push_id_u32(LDKUIContext *ctx, uint32_t value);
  /**
   * @brief Push a pointer identity onto the hierarchical UI ID stack.
   * @param ctx UI context used by this operation.
   * @param value Input value or current widget value.
   * @group modules.ui.ids
   */
  LDK_API void ldk_ui_push_id_ptr(LDKUIContext *ctx, void const *value);
  /**
   * @brief Push a string identity onto the hierarchical UI ID stack.
   * @param ctx UI context used by this operation.
   * @param value Input value or current widget value.
   * @group modules.ui.ids
   */
  LDK_API void ldk_ui_push_id_cstr(LDKUIContext *ctx, char const *value);
  /**
   * @brief Remove the most recent UI ID scope.
   * @param ctx UI context used by this operation.
   * @group modules.ui.ids
   */
  LDK_API void ldk_ui_pop_id(LDKUIContext *ctx);

  /**
   * @brief Set disabled state for the next UI item.
   * @param ctx UI context used by this operation.
   * @param disabled Whether interaction is disabled.
   * @group modules.ui.state
   */
  LDK_API void ldk_ui_set_next_disabled(LDKUIContext *ctx, bool disabled);
  /**
   * @brief Request keyboard focus for the next UI item.
   * @param ctx UI context used by this operation.
   * @group modules.ui.state
   */
  LDK_API void ldk_ui_set_next_focus(LDKUIContext *ctx);
  /**
   * @brief Begin a nested disabled-state scope.
   * @param ctx UI context used by this operation.
   * @param disabled Whether interaction is disabled.
   * @group modules.ui.state
   */
  LDK_API void ldk_ui_begin_disabled(LDKUIContext *ctx, bool disabled);
  /**
   * @brief End the current disabled-state scope.
   * @param ctx UI context used by this operation.
   * @group modules.ui.state
   */
  LDK_API void ldk_ui_end_disabled(LDKUIContext *ctx);

  /**
   * @brief Construct a UI rectangle from position and dimensions.
   * @param x X coordinate or component.
   * @param y Y coordinate or component.
   * @param w W used by this operation.
   * @param h H used by this operation.
   * @return The resulting rectangle.
   * @group modules.ui.layout.geometry
   */
  LDK_API LDKUIRect ldk_ui_rect(float x, float y, float w, float h);
  /**
   * @brief Format a floating-point value for UI display.
   * @param buffer Buffer containing the data or receiving formatted output.
   * @param buffer_size Capacity of the buffer in bytes.
   * @param value Input value or current widget value.
   * @group modules.ui.lifecycle
   */
  LDK_API void ldk_ui_format_float(
      char *buffer, u32 buffer_size, float value);

  //----------------------------------------------------------
  // Windows
  //----------------------------------------------------------
  /**
   * @brief Begin a UI window with fixed placement.
   * @param ctx UI context used by this operation.
   * @param title Title shown by the window or dialog.
   * @param rect UI or geometry rectangle.
   * @param flags Bitmask of options controlling this operation.
   * @return The resulting rectangle.
   * @group modules.ui.windows
   */
  LDK_API LDKUIRect ldk_ui_begin_window_fixed(
      LDKUIContext *ctx, char const *title, LDKUIRect rect, u32 flags);
  /**
   * @brief Begin a UI window.
   * @param ctx UI context used by this operation.
   * @param title Title shown by the window or dialog.
   * @param rect UI or geometry rectangle.
   * @param flags Bitmask of options controlling this operation.
   * @return The resulting rectangle.
   * @group modules.ui.windows
   */
  LDK_API LDKUIRect ldk_ui_begin_window(
      LDKUIContext *ctx, char const *title, LDKUIRect rect, u32 flags);
  /**
   * @brief Begin an optionally closable window with editable geometry.
   * @param ctx UI context used by this operation.
   * @param title Title shown by the window or dialog.
   * @param rect UI or geometry rectangle.
   * @param open In/out flag indicating whether the window remains open.
   * @param flags Bitmask of options controlling this operation.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.windows
   */
  LDK_API bool ldk_ui_begin_window_open(LDKUIContext *ctx, char const *title,
      LDKUIRect *rect, bool *open, u32 flags);
  /**
   * @brief Report whether the current window has requested closure.
   * @param ctx UI context used by this operation.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.windows
   */
  LDK_API bool ldk_ui_window_close_requested(LDKUIContext *ctx);
  /**
   * @brief Determine whether cached window content needs rebuilding.
   * @param ctx UI context used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group modules.ui.windows
   */
  LDK_API bool ldk_ui_window_content_should_build(LDKUIContext *ctx);
  /**
   * @brief Request focus for the specified UI window.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @return True if the operation succeeds; false otherwise.
   * @group modules.ui.windows
   */
  LDK_API bool ldk_ui_window_focus(LDKUIContext *ctx, LDKUIId id);
  /**
   * @brief Invalidate cached contents of one UI window.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @group modules.ui.windows
   */
  LDK_API void ldk_ui_window_invalidate(LDKUIContext *ctx, LDKUIId id);
  /**
   * @brief Invalidate cached contents of all UI windows.
   * @param ctx UI context used by this operation.
   * @group modules.ui.windows
   */
  LDK_API void ldk_ui_windows_invalidate(LDKUIContext *ctx);
  /**
   * @brief Finish building the current UI window.
   * @param ctx UI context used by this operation.
   * @group modules.ui.windows
   */
  LDK_API void ldk_ui_end_window(LDKUIContext *ctx);

  //----------------------------------------------------------
  // Layout size hints
  //----------------------------------------------------------
  /**
   * @brief Create a pixel-based UI layout size.
   * @param value Input value or current widget value.
   * @return The resulting size.
   * @group modules.ui.layout.sizing
   */
  LDK_API LDKUILayoutSize ldk_ui_px(float value);
  /**
   * @brief Create a percentage-based UI layout size.
   * @param value Input value or current widget value.
   * @return The resulting size.
   * @group modules.ui.layout.sizing
   */
  LDK_API LDKUILayoutSize ldk_ui_percent(float value);
  /**
   * @brief Create a UI layout size that fills available space.
   * @return The resulting size.
   * @group modules.ui.layout.sizing
   */
  LDK_API LDKUILayoutSize ldk_ui_fill(void);

  /**
   * @brief Set the width constraint for the next UI item.
   * @param ctx UI context used by this operation.
   * @param width Width in pixels or the units expected by the API.
   * @group modules.ui.layout.sizing
   */
  LDK_API void ldk_ui_set_next_width(LDKUIContext *ctx, LDKUILayoutSize width);
  /**
   * @brief Set the height constraint for the next UI item.
   * @param ctx UI context used by this operation.
   * @param height Height in pixels or the units expected by the API.
   * @group modules.ui.layout.sizing
   */
  LDK_API void ldk_ui_set_next_height(
      LDKUIContext *ctx, LDKUILayoutSize height);
  /**
   * @brief Set width and height constraints for the next UI item.
   * @param ctx UI context used by this operation.
   * @param width Width in pixels or the units expected by the API.
   * @param height Height in pixels or the units expected by the API.
   * @group modules.ui.layout.sizing
   */
  LDK_API void ldk_ui_set_next_size(
      LDKUIContext *ctx, LDKUILayoutSize width, LDKUILayoutSize height);
  /**
   * @brief Set the minimum size for the next UI item.
   * @param ctx UI context used by this operation.
   * @param width Width in pixels or the units expected by the API.
   * @param height Height in pixels or the units expected by the API.
   * @group modules.ui.layout.sizing
   */
  LDK_API void ldk_ui_set_next_min_size(
      LDKUIContext *ctx, float width, float height);
  /**
   * @brief Set the layout weight of the next UI item.
   * @param ctx UI context used by this operation.
   * @param weight Weight used by this operation.
   * @group modules.ui.layout.sizing
   */
  LDK_API void ldk_ui_set_next_weight(LDKUIContext *ctx, float weight);

  /**
   * @brief Get the rectangle of the most recently emitted UI item.
   * @param ctx UI context used by this operation.
   * @return The resulting rectangle.
   * @group modules.ui.layout.geometry
   */
  LDK_API LDKUIRect ldk_ui_last_rect(LDKUIContext *ctx);
  /**
   * @brief Get the outer bounding rectangle of the most recent UI item.
   * @param ctx UI context used by this operation.
   * @return The resulting rectangle.
   * @group modules.ui.layout.geometry
   */
  LDK_API LDKUIRect ldk_ui_last_bounding_rect(LDKUIContext *ctx);
  /**
   * @brief Capture the current position in UI layout construction.
   * @param ctx UI context used by this operation.
   * @return The requested value.
   * @group modules.ui.layout.measurement
   */
  LDK_API LDKUIMark ldk_ui_mark(LDKUIContext *ctx);
  /**
   * @brief Measure the bounding rectangle of UI items added after a mark.
   * @param ctx UI context used by this operation.
   * @param mark Mark used by this operation.
   * @return The resulting rectangle.
   * @group modules.ui.layout.measurement
   */
  LDK_API LDKUIRect ldk_ui_measure_from(LDKUIContext *ctx, LDKUIMark mark);

  //----------------------------------------------------------
  // Layout
  //----------------------------------------------------------
  /**
   * @brief Begin a vertical auto-layout scope.
   * @param ctx UI context used by this operation.
   * @group modules.ui.layout.stacks
   */
  LDK_API void ldk_ui_begin_vertical(LDKUIContext *ctx);
  /**
   * @brief Begin a horizontal auto-layout scope.
   * @param ctx UI context used by this operation.
   * @group modules.ui.layout.stacks
   */
  LDK_API void ldk_ui_begin_horizontal(LDKUIContext *ctx);
  /**
   * @brief Close a vertical auto-layout scope.
   * @param ctx UI context used by this operation.
   * @group modules.ui.layout.stacks
   */
  LDK_API void ldk_ui_end_vertical(LDKUIContext *ctx);
  /**
   * @brief Close a horizontal auto-layout scope.
   * @param ctx UI context used by this operation.
   * @group modules.ui.layout.stacks
   */
  LDK_API void ldk_ui_end_horizontal(LDKUIContext *ctx);

  /**
   * Set the current container's uniform content inset in pixels.
   * Call after successfully opening a window, an expanded area, or a
   * vertical/horizontal/scrollview layout, before adding children.
   * Nested containers keep their own padding. Zero removes
   * the inset; negative or nonfinite values are treated as zero.
   * @param ctx UI context used by this operation.
   * @param padding Padding used by this operation.
   * @group modules.ui.layout.spacing
   */
  LDK_API void ldk_ui_set_padding(LDKUIContext *ctx, float padding);

  //----------------------------------------------------------
  // Scroll views
  //----------------------------------------------------------
  /**
   * @brief Begin a scrollable region and restore its scroll position.
   * @param ctx UI context used by this operation.
   * @param scroll Current scroll offset.
   * @param flags Bitmask of options controlling this operation.
   * @return The resulting point.
   * @group modules.ui.scrolling
   */
  LDK_API LDKUIPoint ldk_ui_begin_scrollview(
      LDKUIContext *ctx, LDKUIPoint scroll, u32 flags);
  /**
   * @brief End the active scrollable region.
   * @param ctx UI context used by this operation.
   * @group modules.ui.scrolling
   */
  LDK_API void ldk_ui_end_scrollview(LDKUIContext *ctx);

  //----------------------------------------------------------
  // Areas
  //----------------------------------------------------------

  /**
   * @brief Begin a collapsible UI area with an optional icon.
   * @param ctx UI context used by this operation.
   * @param title Title shown by the window or dialog.
   * @param icon Icon shown by the widget.
   * @param expanded Whether the collapsible item starts expanded.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.areas
   */
  LDK_API bool ldk_ui_begin_area_ex(
      LDKUIContext *ctx, char const *title, LDKUIIcon icon, bool expanded);
  /**
   * @brief Begin a collapsible UI area.
   * @param ctx UI context used by this operation.
   * @param title Title shown by the window or dialog.
   * @param expanded Whether the collapsible item starts expanded.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.areas
   */
  LDK_API bool ldk_ui_begin_area(
      LDKUIContext *ctx, char const *title, bool expanded);
  /** Rectangle of the current area's header, for header action buttons.
   * @param ctx UI context used by this operation.
   * @return The resulting rectangle.
   * @group modules.ui.layout.geometry
   */
  LDK_API LDKUIRect ldk_ui_area_header_rect(LDKUIContext *ctx);
  /**
   * @brief Close the current UI area.
   * @param ctx UI context used by this operation.
   * @group modules.ui.areas
   */
  LDK_API void ldk_ui_end_area(LDKUIContext *ctx);

  //----------------------------------------------------------
  // Popups
  //----------------------------------------------------------
  /**
   * @brief Open a popup identified by a UI ID.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @group modules.ui.popups
   */
  LDK_API void ldk_ui_open_popup(LDKUIContext *ctx, LDKUIId id);
  /**
   * @brief Open a popup at the requested screen position.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param position Position used by the operation.
   * @group modules.ui.popups
   */
  LDK_API void ldk_ui_open_popup_at(
      LDKUIContext *ctx, LDKUIId id, LDKUIPoint position);
  /**
   * @brief Close the popup with the specified UI ID.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @group modules.ui.popups
   */
  LDK_API void ldk_ui_close_popup(LDKUIContext *ctx, LDKUIId id);
  /**
   * @brief Close the active popup.
   * @param ctx UI context used by this operation.
   * @group modules.ui.popups
   */
  LDK_API void ldk_ui_close_current_popup(LDKUIContext *ctx);
  /**
   * @brief Close all open popups.
   * @param ctx UI context used by this operation.
   * @group modules.ui.popups
   */
  LDK_API void ldk_ui_close_all_popups(LDKUIContext *ctx);
  /**
   * @brief Check whether a popup is currently open.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @return True when the queried condition holds; false otherwise.
   * @group modules.ui.popups
   */
  LDK_API bool ldk_ui_popup_is_open(LDKUIContext *ctx, LDKUIId id);
  /**
   * @brief Begin drawing the named popup when it is open.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.popups
   */
  LDK_API bool ldk_ui_begin_popup(LDKUIContext *ctx, LDKUIId id);
  /**
   * @brief Finish drawing the active popup.
   * @param ctx UI context used by this operation.
   * @group modules.ui.popups
   */
  LDK_API void ldk_ui_end_popup(LDKUIContext *ctx);

  //----------------------------------------------------------
  // Layout widget wrappers
  //----------------------------------------------------------
  /**
   * @brief Draw an image UI item using a texture region and size.
   * @param ctx UI context used by this operation.
   * @param texture Texture used by this operation.
   * @param uv Texture coordinates defining the displayed image region.
   * @param size Size of the data or region, in bytes where applicable.
   * @group modules.ui.images
   */
  LDK_API void ldk_ui_image(LDKUIContext *ctx, LDKUITextureHandle texture,
      LDKUIRect uv, LDKUISize size);
  /**
   * @brief Draw an icon UI item.
   * @param ctx UI context used by this operation.
   * @param icon Icon shown by the widget.
   * @group modules.ui.icons
   */
  LDK_API void ldk_ui_icon(LDKUIContext *ctx, LDKUIIcon icon);
  /**
   * @brief Draw an icon button and report its interaction.
   * @param ctx UI context used by this operation.
   * @param icon Icon shown by the widget.
   * @param text Text used by this operation.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.icons
   */
  LDK_API bool ldk_ui_icon_button(
      LDKUIContext *ctx, LDKUIIcon icon, char const *text);
  /**
   * @brief Draw a label UI element.
   * @param ctx UI context used by this operation.
   * @param text Text used by this operation.
   * @group modules.ui.labels
   */
  LDK_API void ldk_ui_label(LDKUIContext *ctx, char const *text);
  /**
   * @brief Draw an icon alongside text.
   * @param ctx UI context used by this operation.
   * @param icon Icon shown by the widget.
   * @param text Text used by this operation.
   * @group modules.ui.icons
   */
  LDK_API void ldk_ui_icon_label(
      LDKUIContext *ctx, LDKUIIcon icon, char const *text);
  /**
   * @brief Draw selectable UI text.
   * @param ctx UI context used by this operation.
   * @param text Text used by this operation.
   * @group modules.ui.tabs
   */
  LDK_API void ldk_ui_selectable_text(
      LDKUIContext *ctx, char const *text);
  /**
   * @brief Draw and interact with a color view control.
   * @param ctx UI context used by this operation.
   * @param color Color value used by the operation.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.colors
   */
  LDK_API bool ldk_ui_color_view(LDKUIContext *ctx, rgba32 color);
  /**
   * @brief Draw and interact with a button control.
   * @param ctx UI context used by this operation.
   * @param text Text used by this operation.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.buttons
   */
  LDK_API bool ldk_ui_button(LDKUIContext *ctx, char const *text);
  /**
   * @brief Draw and interact with a toggle control.
   * @param ctx UI context used by this operation.
   * @param value Input value or current widget value.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.buttons
   */
  LDK_API bool ldk_ui_toggle(LDKUIContext *ctx, bool value);
  /**
   * @brief Draw and interact with a button flat control.
   * @param ctx UI context used by this operation.
   * @param text Text used by this operation.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.buttons
   */
  LDK_API bool ldk_ui_button_flat(LDKUIContext *ctx, char const *text);
  /**
   * @brief Draw and interact with a combo box control.
   * @param ctx UI context used by this operation.
   * @param items Array of items presented by the widget.
   * @param item_count Number of items in the array.
   * @param selected_index Currently selected item index.
   * @return The resulting numeric value.
   * @group modules.ui.controls
   */
  LDK_API u32 ldk_ui_combo_box(LDKUIContext *ctx, const char *const *items,
      u32 item_count, u32 selected_index);
  /**
   * @brief Draw and interact with a slider control.
   * @param ctx UI context used by this operation.
   * @param value Input value or current widget value.
   * @param min_value Minimum permitted value.
   * @param max_value Maximum permitted value.
   * @return The resulting value.
   * @group modules.ui.controls
   */
  LDK_API float ldk_ui_slider(
      LDKUIContext *ctx, float value, float min_value, float max_value);
  /**
   * @brief Draw and interact with a slider input control.
   * @param ctx UI context used by this operation.
   * @param value Input value or current widget value.
   * @param min_value Minimum permitted value.
   * @param max_value Maximum permitted value.
   * @return The resulting value.
   * @group modules.ui.controls
   */
  LDK_API float ldk_ui_slider_input(
      LDKUIContext *ctx, float value, float min_value, float max_value);
  /**
   * @brief Draw a hierarchical tree item with icon and additional options.
   * @param ctx UI context used by this operation.
   * @param title Title shown by the window or dialog.
   * @param icon Icon shown by the widget.
   * @param expanded Whether the collapsible item starts expanded.
   * @param depth Hierarchy depth or depth setting.
   * @param flags Bitmask of options controlling this operation.
   * @return The resulting numeric value.
   * @group modules.ui.trees
   */
  LDK_API u32 ldk_ui_tree_node_ex(LDKUIContext *ctx, char const *title,
      LDKUIIcon icon, bool expanded, u32 depth, u32 flags);
  /**
   * @brief Draw a hierarchical tree item with expand/collapse state.
   * @param ctx UI context used by this operation.
   * @param title Title shown by the window or dialog.
   * @param expanded Whether the collapsible item starts expanded.
   * @param depth Hierarchy depth or depth setting.
   * @param flags Bitmask of options controlling this operation.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.trees
   */
  LDK_API bool ldk_ui_tree_node(LDKUIContext *ctx, char const *title,
      bool expanded, u32 depth, u32 flags);
  /**
   * @brief Draw an editable text box and update its buffer.
   * @param ctx UI context used by this operation.
   * @param buffer Buffer containing the data or receiving formatted output.
   * @param buffer_size Capacity of the buffer in bytes.
   * @return The resulting numeric value.
   * @group modules.ui.controls
   */
  LDK_API u32 ldk_ui_input_box(
      LDKUIContext *ctx, char *buffer, u32 buffer_size);
  /**
   * @brief Draw an editable text label and update its buffer.
   * @param ctx UI context used by this operation.
   * @param buffer Buffer containing the data or receiving formatted output.
   * @param buffer_size Capacity of the buffer in bytes.
   * @return The resulting numeric value.
   * @group modules.ui.labels
   */
  LDK_API u32 ldk_ui_input_label(
      LDKUIContext *ctx, char *buffer, u32 buffer_size);
  /**
   * @brief Draw a horizontal line UI element.
   * @param ctx UI context used by this operation.
   * @group modules.ui.layout.stacks
   */
  LDK_API void ldk_ui_horizontal_line(LDKUIContext *ctx);
  /**
   * @brief Draw and interact with a resize handle horizontal control.
   * @param ctx UI context used by this operation.
   * @param value Input value or current widget value.
   * @param min_value Minimum permitted value.
   * @param max_value Maximum permitted value.
   * @return The resulting value.
   * @group modules.ui.layout.sizing
   */
  LDK_API float ldk_ui_resize_handle_horizontal(LDKUIContext *ctx,
      float value, float min_value, float max_value);
  /**
   * @brief Draw and interact with a resize handle vertical control.
   * @param ctx UI context used by this operation.
   * @param value Input value or current widget value.
   * @param min_value Minimum permitted value.
   * @param max_value Maximum permitted value.
   * @return The resulting value.
   * @group modules.ui.layout.sizing
   */
  LDK_API float ldk_ui_resize_handle_vertical(LDKUIContext *ctx,
      float value, float min_value, float max_value);
  /**
   * @brief Draw a spacer UI element.
   * @param ctx UI context used by this operation.
   * @group modules.ui.layout.spacing
   */
  LDK_API void ldk_ui_spacer(LDKUIContext *ctx);
  /**
   * @brief Draw and interact with a tab bar control.
   * @param ctx UI context used by this operation.
   * @param items Array of items presented by the widget.
   * @param item_count Number of items in the array.
   * @param active_index Currently active item index.
   * @return Result of the operation.
   * @group modules.ui.tabs
   */
  LDK_API LDKUITabBarResult ldk_ui_tab_bar(LDKUIContext *ctx,
      LDKUITabBarItem const *items, u32 item_count, u32 active_index);

  //----------------------------------------------------------
  // Base widgets
  //----------------------------------------------------------
  /**
   * @brief Draw a UI panel at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param rect UI or geometry rectangle.
   * @group modules.ui.widgets.panels
   */
  LDK_API void ldk_ui_widget_panel(
      LDKUIContext *ctx, LDKUIId id, LDKUIRect rect);
  /**
   * @brief Draw UI text at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param text Text used by this operation.
   * @param rect UI or geometry rectangle.
   * @group modules.ui.widgets.labels
   */
  LDK_API void ldk_ui_widget_label(
      LDKUIContext *ctx, LDKUIId id, char const *text, LDKUIRect rect);
  /**
   * @brief Draw an icon label UI element.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param icon Icon shown by the widget.
   * @param text Text used by this operation.
   * @param rect UI or geometry rectangle.
   * @group modules.ui.widgets.icons
   */
  LDK_API void ldk_ui_widget_icon_label(LDKUIContext *ctx, LDKUIId id,
      LDKUIIcon icon, char const *text, LDKUIRect rect);
  /**
   * @brief Draw a UI image at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param texture Texture used by this operation.
   * @param uv Texture coordinates defining the displayed image region.
   * @param rect UI or geometry rectangle.
   * @group modules.ui.widgets.images
   */
  LDK_API void ldk_ui_widget_image(LDKUIContext *ctx, LDKUIId id,
      LDKUITextureHandle texture, LDKUIRect uv, LDKUIRect rect);
  /**
   * @brief Draw a four-corner UI color gradient.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param top_left Top left used by this operation.
   * @param top_right Top right used by this operation.
   * @param bottom_right Bottom right used by this operation.
   * @param bottom_left Bottom left used by this operation.
   * @param rect UI or geometry rectangle.
   * @group modules.ui.widgets.colors
   */
  LDK_API void ldk_ui_widget_gradient(LDKUIContext *ctx, LDKUIId id,
      rgba32 top_left, rgba32 top_right, rgba32 bottom_right,
      rgba32 bottom_left, LDKUIRect rect);
  /**
   * @brief Draw and interact with a color view control.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param color Color value used by the operation.
   * @param rect UI or geometry rectangle.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.widgets.colors
   */
  LDK_API bool ldk_ui_widget_color_view(
    LDKUIContext *ctx, LDKUIId id, rgba32 color, LDKUIRect rect);
  /**
   * @brief Draw and interact with a button control.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param text Text used by this operation.
   * @param rect UI or geometry rectangle.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.widgets.buttons
   */
  LDK_API bool ldk_ui_widget_button(
      LDKUIContext *ctx, LDKUIId id, char const *text, LDKUIRect rect);
  /**
   * @brief Draw and interact with an icon button control.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param icon Icon shown by the widget.
   * @param text Text used by this operation.
   * @param rect UI or geometry rectangle.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.widgets.icons
   */
  LDK_API bool ldk_ui_widget_icon_button(LDKUIContext *ctx, LDKUIId id,
      LDKUIIcon icon, char const *text, LDKUIRect rect);
  /**
   * @brief Draw and interact with an icon button flat control.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param icon Icon shown by the widget.
   * @param text Text used by this operation.
   * @param rect UI or geometry rectangle.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.widgets.icons
   */
  LDK_API bool ldk_ui_widget_icon_button_flat(LDKUIContext *ctx, LDKUIId id,
      LDKUIIcon icon, char const *text, LDKUIRect rect);
  /**
   * @brief Draw and interact with a button flat control.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param text Text used by this operation.
   * @param rect UI or geometry rectangle.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.widgets.buttons
   */
  LDK_API bool ldk_ui_widget_button_flat(
      LDKUIContext *ctx, LDKUIId id, char const *text, LDKUIRect rect);
  /**
   * @brief Draw a boolean toggle at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param value Input value or current widget value.
   * @param rect UI or geometry rectangle.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.widgets.buttons
   */
  LDK_API bool ldk_ui_widget_toggle(
      LDKUIContext *ctx, LDKUIId id, bool value, LDKUIRect rect);
  /* Explicit-rectangle variant of ldk_ui_combo_box; shares the same
   * rendering, popup behavior and selection semantics. */
  /**
   * @brief Draw and interact with a combo box control.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param items Array of items presented by the widget.
   * @param item_count Number of items in the array.
   * @param selected_index Currently selected item index.
   * @param rect UI or geometry rectangle.
   * @return The resulting numeric value.
   * @group modules.ui.widgets.controls
   */
  LDK_API u32 ldk_ui_widget_combo_box(LDKUIContext *ctx, LDKUIId id,
      const char *const *items, u32 item_count, u32 selected_index,
      LDKUIRect rect);
  /**
   * @brief Draw a numeric slider at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param value Input value or current widget value.
   * @param min_value Minimum permitted value.
   * @param max_value Maximum permitted value.
   * @param rect UI or geometry rectangle.
   * @return The resulting value.
   * @group modules.ui.widgets.controls
   */
  LDK_API float ldk_ui_widget_slider(LDKUIContext *ctx, LDKUIId id, float value,
      float min_value, float max_value, LDKUIRect rect);
  /**
   * @brief Draw an editable numeric slider at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param value Input value or current widget value.
   * @param min_value Minimum permitted value.
   * @param max_value Maximum permitted value.
   * @param rect UI or geometry rectangle.
   * @return The resulting value.
   * @group modules.ui.widgets.controls
   */
  LDK_API float ldk_ui_widget_slider_input(LDKUIContext *ctx, LDKUIId id,
      float value, float min_value, float max_value, LDKUIRect rect);
  /**
   * @brief Draw a vertical scrollbar at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param scroll Current scroll offset.
   * @param visible_size Length of the visible region.
   * @param content_size Length of the scrollable content.
   * @param rect UI or geometry rectangle.
   * @return The resulting value.
   * @group modules.ui.widgets.layout.stacks
   */
  LDK_API float ldk_ui_widget_scrollbar_vertical(LDKUIContext *ctx, LDKUIId id,
      float scroll, float visible_size, float content_size, LDKUIRect rect);
  /**
   * @brief Draw a horizontal scrollbar at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param scroll Current scroll offset.
   * @param visible_size Length of the visible region.
   * @param content_size Length of the scrollable content.
   * @param rect UI or geometry rectangle.
   * @return The resulting value.
   * @group modules.ui.widgets.layout.stacks
   */
  LDK_API float ldk_ui_widget_scrollbar_horizontal(LDKUIContext *ctx,
      LDKUIId id, float scroll, float visible_size, float content_size,
      LDKUIRect rect);
  /**
   * @brief Draw an editable text box at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param buffer Buffer containing the data or receiving formatted output.
   * @param buffer_size Capacity of the buffer in bytes.
   * @param rect UI or geometry rectangle.
   * @return The resulting numeric value.
   * @group modules.ui.widgets.controls
   */
  LDK_API u32 ldk_ui_widget_input_box(LDKUIContext *ctx, LDKUIId id,
      char *buffer, u32 buffer_size, LDKUIRect rect);
  /**
   * @brief Draw an editable text label at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param buffer Buffer containing the data or receiving formatted output.
   * @param buffer_size Capacity of the buffer in bytes.
   * @param rect UI or geometry rectangle.
   * @return The resulting numeric value.
   * @group modules.ui.widgets.labels
   */
  LDK_API u32 ldk_ui_widget_input_label(LDKUIContext *ctx, LDKUIId id,
      char *buffer, u32 buffer_size, LDKUIRect rect);
  /**
   * @brief Draw a tab UI item at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param icon Icon shown by the widget.
   * @param text Text used by this operation.
   * @param rect UI or geometry rectangle.
   * @param active Active used by this operation.
   * @return True when the UI condition or interaction is active; false otherwise.
   * @group modules.ui.widgets.tabs
   */
  LDK_API bool ldk_ui_widget_tab(LDKUIContext *ctx, LDKUIId id, LDKUIIcon icon,
      char const *text, LDKUIRect rect, bool active);
  /**
   * @brief Draw a horizontal resize handle at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param value Input value or current widget value.
   * @param min_value Minimum permitted value.
   * @param max_value Maximum permitted value.
   * @param rect UI or geometry rectangle.
   * @return The resulting value.
   * @group modules.ui.widgets.layout.sizing
   */
  LDK_API float ldk_ui_widget_resize_handle_horizontal(LDKUIContext *ctx,
      LDKUIId id, float value, float min_value, float max_value,
      LDKUIRect rect);
  /**
   * @brief Draw a vertical resize handle at an explicit rectangle.
   * @param ctx UI context used by this operation.
   * @param id Identifier of the target object.
   * @param value Input value or current widget value.
   * @param min_value Minimum permitted value.
   * @param max_value Maximum permitted value.
   * @param rect UI or geometry rectangle.
   * @return The resulting value.
   * @group modules.ui.widgets.layout.sizing
   */
  LDK_API float ldk_ui_widget_resize_handle_vertical(LDKUIContext *ctx,
      LDKUIId id, float value, float min_value, float max_value,
      LDKUIRect rect);

  //----------------------------------------------------------
  // Theme IO
  //----------------------------------------------------------
  /** A resolved UI theme and its display name. Contains no owned resources.
   * @group modules.ui.theme
   */
  typedef struct LDKUIThemeFile
  {
    LDKUITheme theme;
    char name[128];
  } LDKUIThemeFile;

  /**
   * Parse an LDK theme from a null-terminated TML string.
   *
   * The output is replaced only on success. The theme inherits the selected
   * built-in palette, and its icon handles are not loaded from the document.
   * On failure, error receives a diagnostic when a buffer is provided.
   * @param source Source object or source data for this operation.
   * @param out_theme Receives the parsed UI theme.
   * @param error Receives an error description when parsing fails.
   * @param error_size Capacity of the error buffer, in bytes.
   * @return True if the operation succeeds; false otherwise.
   * @group modules.ui.theme
   */
  LDK_API bool ldk_ui_theme_tml_parse(char const *source,
      LDKUIThemeFile *out_theme, char *error, size_t error_size);

  /**
   * Read and parse a UTF-8 TML theme file.
   *
   * The path is an ordinary filesystem path; discovery and application of the
   * theme are separate responsibilities. The output is unchanged on failure.
   * @param path Path identifying the file or resource.
   * @param out_theme Receives the parsed UI theme.
   * @param error Receives an error description when parsing fails.
   * @param error_size Capacity of the error buffer, in bytes.
   * @return True if the operation succeeds; false otherwise.
   * @group modules.ui.theme
   */
  LDK_API bool ldk_ui_theme_tml_load(char const *path,
      LDKUIThemeFile *out_theme, char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif

