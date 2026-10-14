/**
 * # Two-dimensional geometry
 *
 * Geometry and point/rect/size helper types
 *
 * Defines basic geometry types and constructors used across LDK.
 */

#ifndef LDK_GEOM_H
#define LDK_GEOM_H

#include <ldk_common.h>

// LDKSize
/** @group ldk.geom */
typedef struct
{
  i32 w;
  i32 h;
} LDKSize;

/**
 * @brief Construct an integer size from width and height.
 * @param width Width in pixels or the units expected by the API.
 * @param height Height in pixels or the units expected by the API.
 * @return The resulting size.
 * @group ldk.geom
 */
LDK_API LDKSize ldk_size(i32 width, i32 height);
/**
 * @brief Return an integer size with both dimensions zero.
 * @return The resulting size.
 * @group ldk.geom
 */
LDK_API LDKSize ldk_size_zero();
/**
 * @brief Return an integer size with both dimensions one.
 * @return The resulting size.
 * @group ldk.geom
 */
LDK_API LDKSize ldk_size_one();

/** @group ldk.geom */
typedef struct
{
  float w;
  float h;
} LDKSizef;

/**
 * @brief Construct a floating-point size from width and height.
 * @param width Width in pixels or the units expected by the API.
 * @param height Height in pixels or the units expected by the API.
 * @return The resulting size.
 * @group ldk.geom
 */
LDK_API LDKSizef ldk_sizef(float width, float height);
/**
 * @brief Return a floating-point size with both dimensions zero.
 * @return The resulting size.
 * @group ldk.geom
 */
LDK_API LDKSizef ldk_sizef_zero();
/**
 * @brief Return a floating-point size with both dimensions one.
 * @return The resulting size.
 * @group ldk.geom
 */
LDK_API LDKSizef ldk_sizef_one();


// LDKRect
/** @group ldk.geom */
typedef struct 
{
  i32 x;
  i32 y;
  i32 w;
  i32 h;
} LDKRect;

/**
 * @brief Construct an integer rectangle from position and size.
 * @param x X coordinate or component.
 * @param y Y coordinate or component.
 * @param width Width in pixels or the units expected by the API.
 * @param height Height in pixels or the units expected by the API.
 * @return The resulting rectangle.
 * @group ldk.geom
 */
LDK_API LDKRect ldk_rect(i32 x, i32 y, i32 width, i32 height);
/**
 * @brief Test whether the rect contains the requested value.
 * @param rect UI or geometry rectangle.
 * @param x X coordinate or component.
 * @param y Y coordinate or component.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.geom
 */
LDK_API bool ldk_rect_contains(const LDKRect* rect, i32 x, i32 y);
/**
 * @brief Compute the intersection of the rect.
 * @param a First value to compare or combine.
 * @param b Second value to compare or combine.
 * @return The resulting rectangle.
 * @group ldk.geom
 */
LDK_API LDKRect ldk_rect_intersect(const LDKRect* a, const LDKRect* b);

// LDKRectf
/** @group ldk.geom */
typedef struct 
{
  float x;
  float y;
  float w;
  float h;
} LDKRectf;


/**
 * @brief Construct a floating-point rectangle from position and size.
 * @param x X coordinate or component.
 * @param y Y coordinate or component.
 * @param width Width in pixels or the units expected by the API.
 * @param height Height in pixels or the units expected by the API.
 * @return The resulting rectangle.
 * @group ldk.geom
 */
LDK_API LDKRectf ldk_rectf(float x, float y, float width, float height);
/**
 * @brief Test whether the rectf contains the requested value.
 * @param rect UI or geometry rectangle.
 * @param x X coordinate or component.
 * @param y Y coordinate or component.
 * @return True when the queried condition holds; false otherwise.
 * @group ldk.geom
 */
LDK_API bool ldk_rectf_contains(const LDKRectf* rect, float x, float y);
/**
 * @brief Compute the intersection of the rectf.
 * @param a First value to compare or combine.
 * @param b Second value to compare or combine.
 * @return The resulting rectangle.
 * @group ldk.geom
 */
LDK_API LDKRectf ldk_rectf_intersect(const LDKRectf* a, const LDKRectf* b);

// LDKPoint
/** @group ldk.geom */
typedef struct
{
  i32 x;
  i32 y;
} LDKPoint;

/**
 * @brief Construct an integer two-dimensional point.
 * @param x X coordinate or component.
 * @param y Y coordinate or component.
 * @return The resulting point.
 * @group ldk.geom
 */
LDK_API LDKPoint ldk_point(i32 x, i32 y);

// LDKPointf
/** @group ldk.geom */
typedef struct
{
  float x;
  float y;
} LDKPointf;

/**
 * @brief Construct a floating-point two-dimensional point.
 * @param x X coordinate or component.
 * @param y Y coordinate or component.
 * @return The resulting point.
 * @group ldk.geom
 */
LDK_API LDKPointf ldk_pointf(float x, float y);
#endif //LDK_GEOM_H

