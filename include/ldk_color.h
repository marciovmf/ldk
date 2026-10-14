/**
 * # Color values
 *
 * RGB and RGBA color types and helpers
 *
 * Defines color structures, constructors, and named color constants.
 */

#ifndef LDK_COLOR_H
#define LDK_COLOR_H

#include <ldk_common.h>

// LDKRGB
/** @group ldk.color */
typedef struct
{
  u8 r;
  u8 g;
  u8 b;
} LDKRGB;

/**
 * @brief Construct a color value from individual channel components.
 * @param r R color channel.
 * @param g G color channel.
 * @param b Second value to compare or combine.
 * @return The requested value.
 * @group ldk.color
 */
LDK_API LDKRGB ldkRGB(u8 r, u8 g, u8 b);

// LDKRGBA
/** @group ldk.color */
typedef struct
{
  u8 r;
  u8 g;
  u8 b;
  u8 a;
} LDKRGBA;

/**
 * @brief Construct a color value from individual channel components.
 * @param r R color channel.
 * @param g G color channel.
 * @param b Second value to compare or combine.
 * @param a First value to compare or combine.
 * @return The requested value.
 * @group ldk.color
 */
LDK_API LDKRGBA ldkRGBA(u8 r, u8 g, u8 b, u8 a);

/** @group ldk.color */
#define LDK_RGBA_WHITE       ldkRGBA(255, 255, 255, 255)
/** @group ldk.color */
#define LDK_RGBA_BLACK       ldkRGBA(0, 0, 0, 255)
/** @group ldk.color */
#define LDK_RGBA_RED         ldkRGBA(255, 0, 0, 255)
/** @group ldk.color */
#define LDK_RGBA_GREEN       ldkRGBA(0, 255, 0, 255)
/** @group ldk.color */
#define LDK_RGBA_BLUE        ldkRGBA(0, 0, 255, 255)
/** @group ldk.color */
#define LDK_RGBA_YELLOW      ldkRGBA(255, 255, 0, 255)
/** @group ldk.color */
#define LDK_RGBA_CYAN        ldkRGBA(0, 255, 255, 255)
/** @group ldk.color */
#define LDK_RGBA_MAGENTA     ldkRGBA(255, 0, 255, 255)
/** @group ldk.color */
#define LDK_RGBA_ORANGE      ldkRGBA(255, 165, 0, 255)
/** @group ldk.color */
#define LDK_RGBA_PURPLE      ldkRGBA(128, 0, 128, 255)
/** @group ldk.color */
#define LDK_RGBA_GRAY        ldkRGBA(128, 128, 128, 255)
/** @group ldk.color */
#define LDK_RGBA_DARKGRAY    ldkRGBA(64, 64, 64, 255)
/** @group ldk.color */
#define LDK_RGBA_LIGHTGRAY   ldkRGBA(192, 192, 192, 255)
/** @group ldk.color */
#define LDK_RGBA_PINK        ldkRGBA(255, 192, 203, 255)
/** @group ldk.color */
#define LDK_RGBA_BROWN       ldkRGBA(165, 42, 42, 255)
/** @group ldk.color */
#define LDK_RGBA_GOLD        ldkRGBA(255, 215, 0, 255)
/** @group ldk.color */
#define LDK_RGBA_SILVER      ldkRGBA(192, 192, 192, 255)
/** @group ldk.color */
#define LDK_RGBA_LIME        ldkRGBA(0, 255, 0, 255)
/** @group ldk.color */
#define LDK_RGBA_TURQUOISE   ldkRGBA(64, 224, 208, 255)
/** @group ldk.color */
#define LDK_RGBA_TEAL        ldkRGBA(0, 128, 128, 255)
/** @group ldk.color */
#define LDK_RGBA_INDIGO      ldkRGBA(75, 0, 130, 255)
/** @group ldk.color */
#define LDK_RGBA_VIOLET      ldkRGBA(238, 130, 238, 255)
/** @group ldk.color */
#define LDK_RGBA_CORAL       ldkRGBA(255, 127, 80, 255)
/** @group ldk.color */
#define LDK_RGBA_CHOCOLATE   ldkRGBA(210, 105, 30, 255)
/** @group ldk.color */
#define LDK_RGBA_IVORY       ldkRGBA(255, 255, 240, 255)
/** @group ldk.color */
#define LDK_RGBA_BEIGE       ldkRGBA(245, 222, 179, 255)
/** @group ldk.color */
#define LDK_RGBA_MINT        ldkRGBA(189, 252, 201, 255)
/** @group ldk.color */
#define LDK_RGBA_LAVENDER    ldkRGBA(230, 230, 250, 255)
/** @group ldk.color */
#define LDK_RGBA_PEACH       ldkRGBA(255, 229, 180, 255)
/** @group ldk.color */
#define LDK_RGBA_SEASHELL    ldkRGBA(255, 228, 196, 255)
/** @group ldk.color */
#define LDK_RGBA_SALMON      ldkRGBA(250, 128, 114, 255)
/** @group ldk.color */
#define LDK_RGBA_PLUM        ldkRGBA(221, 160, 221, 255)
/** @group ldk.color */
#define LDK_RGBA_MAROON      ldkRGBA(128, 0, 0, 255)
/** @group ldk.color */
#define LDK_RGBA_OLIVE       ldkRGBA(128, 128, 0, 255)
/** @group ldk.color */
#define LDK_RGBA_NAVY        ldkRGBA(0, 0, 128, 255)
/** @group ldk.color */
#define LDK_RGBA_FUCHSIA     ldkRGBA(255, 0, 255, 255)
/** @group ldk.color */
#define LDK_RGBA_KHAKI       ldkRGBA(240, 230, 140, 255)
/** @group ldk.color */
#define LDK_RGBA_SLATEGRAY   ldkRGBA(112, 128, 144, 255)
/** @group ldk.color */
#define LDK_RGBA_AQUA        ldkRGBA(0, 255, 255, 255)
/** @group ldk.color */
#define LDK_RGBA_LIGHTBLUE   ldkRGBA(173, 216, 230, 255)
/** @group ldk.color */
#define LDK_RGBA_DODGERBLUE  ldkRGBA(30, 144, 255, 255)

/** @group ldk.color */
#define LDK_RGB_WHITE       ldkRGB(255, 255, 255)
/** @group ldk.color */
#define LDK_RGB_BLACK       ldkRGB(0, 0, 0)
/** @group ldk.color */
#define LDK_RGB_RED         ldkRGB(255, 0, 0)
/** @group ldk.color */
#define LDK_RGB_GREEN       ldkRGB(0, 255, 0)
/** @group ldk.color */
#define LDK_RGB_BLUE        ldkRGB(0, 0, 255)
/** @group ldk.color */
#define LDK_RGB_YELLOW      ldkRGB(255, 255, 0)
/** @group ldk.color */
#define LDK_RGB_CYAN        ldkRGB(0, 255, 255)
/** @group ldk.color */
#define LDK_RGB_MAGENTA     ldkRGB(255, 0, 255)
/** @group ldk.color */
#define LDK_RGB_ORANGE      ldkRGB(255, 165, 0)
/** @group ldk.color */
#define LDK_RGB_PURPLE      ldkRGB(128, 0, 128)
/** @group ldk.color */
#define LDK_RGB_GRAY        ldkRGB(128, 128, 128)
/** @group ldk.color */
#define LDK_RGB_DARKGRAY    ldkRGB(64, 64, 64)
/** @group ldk.color */
#define LDK_RGB_LIGHTGRAY   ldkRGB(192, 192, 192)
/** @group ldk.color */
#define LDK_RGB_PINK        ldkRGB(255, 192, 203)
/** @group ldk.color */
#define LDK_RGB_BROWN       ldkRGB(165, 42, 42)
/** @group ldk.color */
#define LDK_RGB_GOLD        ldkRGB(255, 215, 0)
/** @group ldk.color */
#define LDK_RGB_SILVER      ldkRGB(192, 192, 192)
/** @group ldk.color */
#define LDK_RGB_LIME        ldkRGB(0, 255, 0)
/** @group ldk.color */
#define LDK_RGB_TURQUOISE   ldkRGB(64, 224, 208)
/** @group ldk.color */
#define LDK_RGB_TEAL        ldkRGB(0, 128, 128)
/** @group ldk.color */
#define LDK_RGB_INDIGO      ldkRGB(75, 0, 130)
/** @group ldk.color */
#define LDK_RGB_VIOLET      ldkRGB(238, 130, 238)
/** @group ldk.color */
#define LDK_RGB_CORAL       ldkRGB(255, 127, 80)
/** @group ldk.color */
#define LDK_RGB_CHOCOLATE   ldkRGB(210, 105, 30)
/** @group ldk.color */
#define LDK_RGB_IVORY       ldkRGB(255, 255, 240)
/** @group ldk.color */
#define LDK_RGB_BEIGE       ldkRGB(245, 222, 179)
/** @group ldk.color */
#define LDK_RGB_MINT        ldkRGB(189, 252, 201)
/** @group ldk.color */
#define LDK_RGB_LAVENDER    ldkRGB(230, 230, 250)
/** @group ldk.color */
#define LDK_RGB_PEACH       ldkRGB(255, 229, 180)
/** @group ldk.color */
#define LDK_RGB_SEASHELL    ldkRGB(255, 228, 196)
/** @group ldk.color */
#define LDK_RGB_SALMON      ldkRGB(250, 128, 114)
/** @group ldk.color */
#define LDK_RGB_PLUM        ldkRGB(221, 160, 221)
/** @group ldk.color */
#define LDK_RGB_MAROON      ldkRGB(128, 0, 0)
/** @group ldk.color */
#define LDK_RGB_OLIVE       ldkRGB(128, 128, 0)
/** @group ldk.color */
#define LDK_RGB_NAVY        ldkRGB(0, 0, 128)
/** @group ldk.color */
#define LDK_RGB_FUCHSIA     ldkRGB(255, 0, 255)
/** @group ldk.color */
#define LDK_RGB_KHAKI       ldkRGB(240, 230, 140)
/** @group ldk.color */
#define LDK_RGB_SLATEGRAY   ldkRGB(112, 128, 144)
/** @group ldk.color */
#define LDK_RGB_AQUA        ldkRGB(0, 255, 255)
/** @group ldk.color */
#define LDK_RGB_LIGHTBLUE   ldkRGB(173, 216, 230)
/** @group ldk.color */
#define LDK_RGB_DODGERBLUE  ldkRGB(30, 144, 255)
#endif //LDK_COLOR_H

