#include "ldk/maths.h"
#include "common.h"
#define _USE_MATH_DEFINES
#include <math.h>

//
// Float operations
//


float clamp(float x, float bottom, float top)
{
  x = fmaxf(x, bottom);
  x = fminf(x, top);
  return x;
}

bool between(float x, float bottom, float top)
{
  return (x > bottom) && (x < top);
}

float lerp(float p1, float p2, float amount)
{
  return (p2 * amount) + (p1 * (1-amount));
}

float smoothstep(float p1, float p2, float amount)
{
  float scaledAmount = amount*amount*(3 - 2*amount);
  return lerp( p1, p2, scaledAmount );
}

double degToRadian(double deg)
{
  return deg * (M_PI / 180);
}

double radianToDeg(double radian)
{
  return radian * (180.0 / M_PI);
}

int32 floatsAreEqualEpsilon(float a, float b, float epsilon)
{
  return fabs(a - b) < epsilon;
}

int32 floatIsZeroEpsilon(float a, float epsilon)
{
  return (a > -epsilon && a < epsilon);
}

int32 floatIsNegativeEpsilon(float a, float epsilon)
{
  return a < -epsilon;
}

int32 floatIsPositiveEpsilon(float a, float epsilon)
{
  return !floatIsNegativeEpsilon(a, epsilon);
}

int32 floatIsGreaterThanEpsilon(float a, float b, float epsilon)
{
  return (a - b) > epsilon;
}

int32 floatIsLessThanEpsilon(float a, float b, float epsilon)
{
  return (b - a) > epsilon;
}

int32 floatIsGreaterThanOrEqualEpsilon(float a, float b, float epsilon)
{
  return !floatIsLessThanEpsilon(a, b, epsilon);
}

int32 floatIssLessThanOrEqual(float a, float b, float epsilon)
{
  return !floatIsGreaterThanEpsilon(a, b, epsilon);
}

int32 floatsAreAlmostEqualRelativeEpsilon(float a, float b, float relativeEpsilon)
{
  if (floatsAreEqualEpsilon(a, b, relativeEpsilon))
    return 1;
  // Use relative difference
  double maxAbs = fmax(fabs(a), fabs(b));
  return fabs(a - b) <= relativeEpsilon * maxAbs;
}

bool floatIsMultipleEpsilon(float a, float b, float epsilon)
{
  if (floatIsZeroEpsilon(b, LDK_EPSILON))
    return false;

  double result = a / b;
  return (fabs(result - round(result)) < epsilon);
}

//
// Vec2
//

Vec2 vec2(float x, float y)
{
  Vec2 v;
  v.x = x;
  v.y = y;
  return v;
}

Vec2 vec2Zero(void)
{
  return vec2(0, 0);
}

Vec2 vec2One(void)
{
  return vec2(1, 1);
}

Vec2 vec2Add(Vec2 v1, Vec2 v2)
{
  Vec2 v;
  v.x = v1.x + v2.x;
  v.y = v1.y + v2.y;
  return v;
}

Vec2 vec2Sub(Vec2 v1, Vec2 v2)
{
  Vec2 v;
  v.x = v1.x - v2.x;
  v.y = v1.y - v2.y;
  return v;
}

Vec2 vec2Div(Vec2 v, float fac)
{
  v.x = v.x / fac;
  v.y = v.y / fac;
  return v;
}

Vec2 vec2DivVec2(Vec2 v1, Vec2 v2)
{
  v1.x = v1.x / v2.x;
  v1.y = v1.y / v2.y;
  return v1;
}

Vec2 vec2Mul(Vec2 v, float fac)
{
  v.x = v.x * fac;
  v.y = v.y * fac;
  return v;
}

Vec2 vec2MulVec2(Vec2 v1, Vec2 v2)
{
  Vec2 v;
  v.x = v1.x * v2.x;
  v.y = v1.y * v2.y;
  return v;
}

Vec2 vec2Pow(Vec2 v, float exp)
{
  v.x = (float) pow(v.x, exp);
  v.y = (float) pow(v.y, exp);
  return v;
}

Vec2 vec2Neg(Vec2 v)
{
  v.x = -v.x;
  v.y = -v.y;
  return v;
}

Vec2 vec2Abs(Vec2 v)
{
  v.x = (float) fabs(v.x);
  v.y = (float) fabs(v.y);
  return v;
}

Vec2 vec2Floor(Vec2 v)
{
  v.x = (float) floor(v.x);
  v.y = (float) floor(v.y);
  return v;
}

Vec2 vec2Fmod(Vec2 v, float val)
{
  v.x = (float) fmod(v.x, val);
  v.y = (float) fmod(v.y, val);
  return v;
}

Vec2 vec2fmaxf(Vec2 v, float x)
{
  v.x = fmaxf(v.x, x);
  v.y = fmaxf(v.y, x);
  return v;
}

Vec2 vec2fminf(Vec2 v, float x)
{
  v.x = fminf(v.x, x);
  v.y = fminf(v.y, x);
  return v;
}

Vec2 vec2Clamp(Vec2 v, float b, float t)
{
  v.x = clamp(v.x, b, t);
  v.y = clamp(v.y, b, t);
  return v;
}

void vec2Print(Vec2 v)
{
  ldkLogInfo("vec2(%4.2f,%4.2f)", v.x, v.y);
}

float vec2Dot(Vec2 v1, Vec2 v2)
{
  return (v1.x * v2.x) + (v1.y * v2.y);
}

float vec2LengthSqrd(Vec2 v)
{
  float length = 0.0;
  length += v.x * v.x;
  length += v.y * v.y;
  return length;
}

float vec2Length(Vec2 v)
{
  return (float) sqrt(vec2LengthSqrd(v));
}

float vec2DistSqrd(Vec2 v1, Vec2 v2)
{
  return (v1.x - v2.x) * (v1.x - v2.x) + 
    (v1.y - v2.y) * (v1.y - v2.y);
}

float vec2Dist(Vec2 v1, Vec2 v2)
{
  return (float) sqrt(vec2DistSqrd(v1, v2));
}

float vec2DistManhattan(Vec2 v1, Vec2 v2)
{
  return (float) (fabs(v1.x - v2.x) + fabs(v1.y - v2.y));
}

Vec2 vec2Normalize(Vec2 v)
{
  float len = vec2Length(v);
  return vec2Div(v, len);
}

Vec2 vec2Reflect(Vec2 v1, Vec2 v2)
{
  return vec2Sub(v1, vec2Mul(v2, 2 * vec2Dot(v1, v2)));
}

bool vec2Equ(Vec2 v1, Vec2 v2)
{
  if(!(v1.x == v2.x))
  { return false; }
  if(!(v1.y == v2.y))
  { return false; }
  return true;
}

void vec2ToArray(Vec2 v, float* out)
{
  out[0] = v.x;
  out[1] = v.y;
}

Vec2 vec2Lerp(Vec2 v1, Vec2 v2, float amount)
{
  Vec2 v;
  v.x = lerp(v1.x, v2.x, amount);
  v.y = lerp(v1.y, v2.y, amount);
  return v;
}

Vec2 vec2Smoothstep(Vec2 v1, Vec2 v2, float amount)
{
  float scaledAmount = amount*amount*(3 - 2*amount);
  return vec2Lerp( v1, v2, scaledAmount );
}


//
// Vec3
//

Vec3 vec3(float x, float y, float z)
{
  Vec3 v;
  v.x = x;
  v.y = y;
  v.z = z;
  return v;
}

Vec3 vec3Zero(void)
{
  return vec3(0, 0, 0);
}

Vec3 vec3One(void)
{
  return vec3(1, 1, 1);
}

Vec3 vec3Up(void)
{
  return vec3(0, 1, 0);
}

Vec3 vec3Right(void)
{
  return vec3(1, 0, 0);
}

Vec3 vec3Forward(void)
{
  return vec3(0, 0, 1);
}

Vec3 vec3Add(Vec3 v1, Vec3 v2)
{
  Vec3 v;
  v.x = v1.x + v2.x;
  v.y = v1.y + v2.y;
  v.z = v1.z + v2.z;
  return v;
}

Vec3 vec3Sub(Vec3 v1, Vec3 v2)
{
  Vec3 v;
  v.x = v1.x - v2.x;
  v.y = v1.y - v2.y;
  v.z = v1.z - v2.z;
  return v;
}

Vec3 vec3Div(Vec3 v, float fac)
{
  v.x = v.x / fac;
  v.y = v.y / fac;
  v.z = v.z / fac;
  return v;
}

Vec3 vec3DivVec3(Vec3 v1, Vec3 v2)
{
  Vec3 v;
  v.x = v1.x / v2.x;
  v.y = v1.y / v2.y;
  v.z = v1.z / v2.z;
  return v;
}

Vec3 vec3Mul(Vec3 v, float fac)
{
  v.x = v.x * fac;
  v.y = v.y * fac;
  v.z = v.z * fac;
  return v;
}

Vec3 vec3MulVec3(Vec3 v1, Vec3 v2)
{
  Vec3 v;
  v.x = v1.x * v2.x;
  v.y = v1.y * v2.y;
  v.z = v1.z * v2.z;
  return v;
}

Vec3 vec3Pow(Vec3 v, float exp)
{
  v.x = (float) pow(v.x, exp);
  v.y = (float) pow(v.y, exp);
  v.z = (float) pow(v.z, exp);
  return v;
}

Vec3 vec3Neg(Vec3 v)
{
  v.x = -v.x;
  v.y = -v.y;
  v.z = -v.z;
  return v;
}

Vec3 vec3Abs(Vec3 v)
{
  v.x = (float) fabs(v.x);
  v.y = (float) fabs(v.y);
  v.z = (float) fabs(v.z);
  return v;
}

Vec3 vec3Floor(Vec3 v)
{
  v.x = (float) floor(v.x);
  v.y = (float) floor(v.y);
  v.z = (float) floor(v.z);
  return v;
}

Vec3 vec3Fmod(Vec3 v, float val)
{
  v.x = (float) fmod(v.x, val);
  v.y = (float) fmod(v.y, val);
  v.z = (float) fmod(v.z, val);
  return v;
}

void vec3Print(Vec3 v)
{
  ldkLogInfo("vec3(%4.2f,%4.2f,%4.2f)", v.x, v.y, v.z);
}

float vec3Dot(Vec3 v1, Vec3 v2)
{
  return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
}

Vec3 vec3Cross(Vec3 v1, Vec3 v2)
{
  Vec3 v;
  v.x = (v1.y * v2.z) - (v1.z * v2.y);
  v.y = (v1.z * v2.x) - (v1.x * v2.z);
  v.z = (v1.x * v2.y) - (v1.y * v2.x);
  return v;
}

float vec3LengthSqrd(Vec3 v)
{
  float length = 0.0;
  length += v.x * v.x;
  length += v.y * v.y;
  length += v.z * v.z;
  return length;
}

float vec3Length(Vec3 v)
{
  return (float) sqrt(vec3LengthSqrd(v));
}

float vec3DistSqrd(Vec3 v1, Vec3 v2)
{
  return (v1.x - v2.x) * (v1.x - v2.x) + 
    (v1.y - v2.y) * (v1.y - v2.y) + 
    (v1.z - v2.z) * (v1.z - v2.z);
}

float vec3Dist(Vec3 v1, Vec3 v2)
{
  return (float) sqrt(vec3DistSqrd(v1, v2));
}

float vec3DistManhattan(Vec3 v1, Vec3 v2)
{
  return (float) (fabs(v1.x - v2.x) + fabs(v1.y - v2.y) + fabs(v1.z - v2.z));
}

Vec3 vec3Normalize(Vec3 v)
{
  float len = vec3Length(v);
  if (len == 0.0)
  {
    return vec3Zero();
  } else {
    return vec3Div(v, len);
  }
}

Vec3 vec3Reflect(Vec3 v1, Vec3 v2)
{
  return vec3Sub(v1, vec3Mul(v2, 2 * vec3Dot(v1, v2)));
}

Vec3 vec3Project(Vec3 v1, Vec3 v2)
{
  return vec3Sub(v1, vec3Mul(v2, vec3Dot(v1, v2)));
}

bool vec3Equ(Vec3 v1, Vec3 v2)
{
  if (v1.x != v2.x)
  { return false; }
  if (v1.y != v2.y)
  { return false; }
  if (v1.z != v2.z)
  { return false; }
  return true;
}

bool vec3Neq(Vec3 v1, Vec3 v2)
{
  if (v1.x != v2.x)
  { return true; }
  if (v1.y != v2.y)
  { return true; }
  if (v1.z != v2.z)
  { return true; }
  return false;
}

Vec3 vec3Lerp(Vec3 v1, Vec3 v2, float amount)
{
  Vec3 v;
  v.x = lerp(v1.x, v2.x, amount);
  v.y = lerp(v1.y, v2.y, amount);
  v.z = lerp(v1.z, v2.z, amount);
  return v;
}

Vec3 vec3Smoothstep(Vec3 v1, Vec3 v2, float amount)
{
  float scaledAmount = amount*amount*(3 - 2*amount);
  return vec3Lerp( v1, v2, scaledAmount );
}

Vec3 rgbToVec3(LDKRGB color)
{
  return vec3(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
}

LDKRGB vec3ToRGB(Vec3 v)
{
  LDKRGB color = ldkRGB(
      (uint8)(v.x * 255.0f),
      (uint8)(v.y * 255.0f),
      (uint8)(v.z * 255.0f));
  return color;
}

float Vec3AngleBetween(Vec3 v1, Vec3 v2)
{
  const float magnitudeV1 = vec3Length(v1);
  const float magnitudeV2 = vec3Length(v2);

  if(floatIsZero(magnitudeV1) || floatIsZero(magnitudeV2))
    return 0;
  const float cosTheta = vec3Dot(v1, v2) / (magnitudeV1 * magnitudeV2);
    float angle = acosf(fmaxf(-1.0f, fminf(1.0f, cosTheta)));
    return angle;
}

float Vec3SignedAngleBetween(Vec3 v1, Vec3 v2, Vec3 normalDirectin)
{
  const float magnitudeV1 = vec3Length(v1);
  const float magnitudeV2 = vec3Length(v2);

  if(floatIsZero(magnitudeV1) || floatIsZero(magnitudeV2))
    return 0;

  const float cosTheta = vec3Dot(v1, v2) / (magnitudeV1 * magnitudeV2);
  float angle = acosf(fmaxf(-1.0f, fminf(1.0f, cosTheta)));
  float signedAngle = vec3Dot(vec3Cross(v1, v2), normalDirectin) < 0.0f ? -angle : angle;
  return signedAngle;
}

//
// Vec4
//

Vec4 vec4(float x, float y, float z, float w)
{
  Vec4 v;
  v.x = x;
  v.y = y;
  v.z = z;
  v.w = w;
  return v;
}

Vec4 vec4Zero(void)
{
  return vec4(0, 0, 0, 0);
}

Vec4 vec4One(void)
{
  return vec4(1, 1, 1, 1);
}

Vec4 vec4Red(void)
{
  return vec4(1,0,0,1);
}

Vec4 vec4Add(Vec4 v1, Vec4 v2)
{
  Vec4 v;
  v.x = v1.x + v2.x;
  v.y = v1.y + v2.y;
  v.z = v1.z + v2.z;
  v.w = v1.w + v2.w;
  return v;
}

Vec4 vec4Sub(Vec4 v1, Vec4 v2)
{
  Vec4 v;
  v.x = v1.x - v2.x;
  v.y = v1.y - v2.y;
  v.z = v1.z - v2.z;
  v.w = v1.w - v2.w;
  return v;
}

Vec4 vec4Div(Vec4 v, float fac)
{
  v.x = v.x / fac;
  v.y = v.y / fac;
  v.z = v.z / fac;
  v.w = v.w / fac;
  return v;
}

Vec4 vec4Mul(Vec4 v, float fac)
{
  v.x = v.x * fac;
  v.y = v.y * fac;
  v.z = v.z * fac;
  v.w = v.w * fac;
  return v;
}

Vec4 vec4MulVec4(Vec4 v1, Vec4 v2)
{
  Vec4 v;
  v.x = v1.x * v2.x;
  v.y = v1.y * v2.y;
  v.z = v1.z * v2.z;
  v.w = v1.w * v2.w;
  return v;
}

Vec4 vec4Pow(Vec4 v, float exp)
{
  v.x = (float) pow(v.x, exp);
  v.y = (float) pow(v.y, exp);
  v.z = (float) pow(v.z, exp);
  v.w = (float) pow(v.w, exp);
  return v;
}

Vec4 vec4Neg(Vec4 v)
{
  v.x = -v.x;
  v.y = -v.y;
  v.z = -v.z;
  v.w = -v.w;
  return v;
}

Vec4 vec4Abs(Vec4 v)
{
  v.x = (float) fabs(v.x);
  v.y = (float) fabs(v.y);
  v.z = (float) fabs(v.z);
  v.w = (float) fabs(v.w);
  return v;
}

Vec4 vec4Floor(Vec4 v)
{
  v.x = (float) floor(v.x);
  v.y = (float) floor(v.y);
  v.z = (float) floor(v.z);
  v.w = (float) floor(v.w);
  return v;
}

Vec4 vec4Fmod(Vec4 v, float val)
{
  v.x = (float) fmod(v.x, val);
  v.y = (float) fmod(v.y, val);
  v.z = (float) fmod(v.z, val);
  v.w = (float) fmod(v.w, val);
  return v;  
}

Vec4 vec4Sqrt(Vec4 v)
{
  v.x = (float) sqrt(v.x);
  v.y = (float) sqrt(v.y);
  v.z = (float) sqrt(v.z);
  v.w = (float) sqrt(v.w);
  return v;  
}

void vec4Print(Vec4 v)
{
  ldkLogInfo("vec4(%4.2f, %4.2f, %4.2f, %4.2f)", v.x, v.y, v.z,  v.w);
}

float vec4Dot(Vec4 v1, Vec4 v2)
{
  return  (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z) + (v1.w * v2.w);
}

float vec4LengthSqrd(Vec4 v)
{
  float length = 0.0;
  length += v.x * v.x;
  length += v.y * v.y;
  length += v.z * v.z;
  length += v.w * v.w;
  return length;
}

float vec4Length(Vec4 v)
{
  return (float) sqrt(vec4LengthSqrd(v));
}

float vec4DistSqrd(Vec4 v1, Vec4 v2)
{
  return (v1.x - v2.x) * (v1.x - v2.x) + 
    (v1.y - v2.y) * (v1.y - v2.y) +
    (v1.z - v2.z) * (v1.z - v2.z) +
    (v1.w - v2.w) * (v1.w - v2.w);
}

float vec4Dist(Vec4 v1, Vec4 v2)
{
  return (float) sqrt(vec4DistSqrd(v1, v2));
}

float vec4DistManhattan(Vec4 v1, Vec4 v2)
{
  return (float) (fabs(v1.x - v2.x) + fabs(v1.y - v2.y) + fabs(v1.z - v2.z) + fabs(v1.w - v2.w));
}

Vec4 vec4Normalize(Vec4 v)
{
  float len = vec4Length(v);
  if (len == 0.0)
  {
    return vec4Zero();
  } else {
    return vec4Div(v, len);
  }
}

Vec4 vec4Reflect(Vec4 v1, Vec4 v2)
{
  return vec4Sub(v1, vec4Mul(v2, 2 * vec4Dot(v1, v2)));
}

Vec4 vec4fmaxf(Vec4 v1, Vec4 v2)
{
  v1.x = fmaxf(v1.x, v2.x);
  v1.y = fmaxf(v1.y, v2.y);
  v1.z = fmaxf(v1.z, v2.z);
  v1.w = fmaxf(v1.w, v2.w);
  return v1;
}

Vec4 vec4fminf(Vec4 v1, Vec4 v2)
{
  v1.x = fminf(v1.x, v2.x);
  v1.y = fminf(v1.y, v2.y);
  v1.z = fminf(v1.z, v2.z);
  v1.w = fminf(v1.w, v2.w);
  return v1;
}

bool vec4Equ(Vec4 v1, Vec4 v2)
{
  if(!(v1.x == v2.x))
  { return false; }
  if(!(v1.y == v2.y))
  { return false; }
  if(!(v1.z == v2.z))
  { return false; }
  if(!(v1.w == v2.w))
  { return false; }
  return true;
}

void vec4ToArray(Vec4 v, float* out)
{
  out[0] = v.x;
  out[1] = v.y;
  out[2] = v.z;
  out[3] = v.w;
}

Vec3 vec4FromHomogeneous(Vec4 v)
{
  Vec3 vec = vec3(v.x,v.y,v.z);
  return vec3Div(vec, v.w);
};

Vec4 vec4Lerp(Vec4 v1, Vec4 v2, float amount)
{
  Vec4 v;
  v.x = lerp(v1.x, v2.x, amount);
  v.y = lerp(v1.y, v2.y, amount);
  v.z = lerp(v1.z, v2.z, amount);
  v.w = lerp(v1.w, v2.w, amount);
  return v;
}

Vec4 vec4Smoothstep(Vec4 v1, Vec4 v2, float amount)
{
  float scaledAmount = amount*amount*(3 - 2*amount);
  return vec4Lerp( v1, v2, scaledAmount );
}


Vec4 rgbaToVec4(LDKRGBA color)
{
  return vec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
}


Vec4 rgbToVec4(LDKRGB color)
{
  return vec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, 1.0f);
}


//
// Quaternion
//

Quat quat(float x, float y, float z, float w)
{
  Quat q;
  q.x = x;
  q.y = y;
  q.z = z;
  q.w = w;
  return q;
}

Quat quatId(void)
{
  return quat(0, 0, 0, 1);
}

float quatAt(Quat q, int i)
{

  float* values = (float*)(&q);
  return values[i];

}

float quatReal(Quat q)
{
  return q.w;
}

Vec3 quatImaginaries(Quat q)
{
  return vec3(q.x, q.y, q.z);
}

Quat quatFromEuler(Vec3 r)
{
  float cy = cosf(r.z * 0.5f);
  float sy = sinf(r.z * 0.5f);
  float cp = cosf(r.y * 0.5f);
  float sp = sinf(r.y * 0.5f);
  float cr = cosf(r.x * 0.5f);
  float sr = sinf(r.x * 0.5f);

  Quat q;
  q.w = cr * cp * cy + sr * sp * sy;
  q.x = sr * cp * cy - cr * sp * sy;
  q.y = cr * sp * cy + sr * cp * sy;
  q.z = cr * cp * sy - sr * sp * cy;
  return q;
}

Quat quatAngleAxis(float angle, Vec3 axis)
{

  float sine = sinf( angle / 2.0f );
  float cosine = cosf( angle / 2.0f );

  return quatNormalize(quat(
        axis.x * sine,
        axis.y * sine,
        axis.z * sine,
        cosine));

}

Quat quatRotationX(float angle)
{
  return quatAngleAxis(angle, vec3(1,0,0));
}

Quat quatRotationY(float angle)
{
  return quatAngleAxis(angle, vec3(0,1,0));
}

Quat quatRotationZ(float angle)
{
  return quatAngleAxis(angle, vec3(0,0,1));
}

void quatToAngleAxis(Quat q, Vec3* axis, float* angle)
{
  *angle = 2.0f * acosf( q.w );

  float divisor = sinf( *angle / 2.0f );

  if( fabs( divisor ) < LDK_EPSILON )
  {

    axis->x = 0.0f;
    axis->y = 1.0f;
    axis->z = 0.0f;

  } else {

    axis->x = q.x / divisor;
    axis->y = q.y / divisor;
    axis->z = q.z / divisor;
    *axis = vec3Normalize(*axis);

  }
}

Vec3 quatToEuler(Quat q)
{
  Vec3 euler;
  // Roll (x-axis rotation)
  float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
  float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
  euler.x = atan2f(sinr_cosp, cosr_cosp);

  // Pitch (y-axis rotation)
  float sinp = 2.0f * (q.w * q.y - q.z * q.x);
  if (fabsf(sinp) >= 1.0f)
    euler.y = copysignf((float)(M_PI / 2.0), sinp); // Use 90 degrees if out of range
  else
    euler.y = asinf(sinp);

  // Yaw (z-axis rotation)
  float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
  float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
  euler.z = atan2f(siny_cosp, cosy_cosp);

  return euler;
}

Quat quatMulQuat(Quat q1, Quat q2)
{

  return quat(
      (q1.w * q2.x) + (q1.x * q2.w) + (q1.y * q2.z) - (q1.z * q2.y),
      (q1.w * q2.y) - (q1.x * q2.z) + (q1.y * q2.w) + (q1.z * q2.x),
      (q1.w * q2.z) + (q1.x * q2.y) - (q1.y * q2.x) + (q1.z * q2.w),
      (q1.w * q2.w) - (q1.x * q2.x) - (q1.y * q2.y) - (q1.z * q2.z));

}

Vec3 quatMulVec3(Quat q, Vec3 v)
{

  Quat work = q;
  work = quatMulQuat(work, quatNormalize(quat(v.x, v.y, v.z, 0.0)));
  work = quatMulQuat(work, quatInverse(q));

  Vec3 res = vec3(work.x, work.y, work.z);

  return vec3Mul(res, vec3Length(v));

}

Quat quatInverse(Quat q)
{

  float	scale = quatLength(q);
  Quat result = quatUnitInverse(q);

  if ( scale > LDK_EPSILON )
  {    
    result.x /= scale;
    result.y /= scale;
    result.z /= scale;
    result.w /= scale;
  }

  return result;
}

Quat quatUnitInverse(Quat q)
{
  return quat(-q.x, -q.y, -q.z, q.w);
}

float quatLength(Quat q)
{
  return sqrtf(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
}

Quat quatNormalize(Quat q)
{
  float scale = quatLength(q);

  if ( scale > LDK_EPSILON )
  {
    return quat(
        q.x / scale,
        q.y / scale,
        q.z / scale,
        q.w / scale);
  } else {
    return quat(0,0,0,0);
  }

}

Quat quatSlerp(Quat from, Quat to, float amount)
{

  float scale0, scale1;
  float	afto1[4];
  float cosom = from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w;

  if ( cosom < 0.0f )
  {
    cosom = -cosom; 
    afto1[0] = -to.x;
    afto1[1] = -to.y;
    afto1[2] = -to.z;
    afto1[3] = -to.w;
  } else {
    afto1[0] = to.x;
    afto1[1] = to.y;
    afto1[2] = to.z;
    afto1[3] = to.w;
  }

  const float QUATERNIONDELTACOSMIN = 0.01f;

  if ( (1.0f - cosom) > QUATERNIONDELTACOSMIN )
  {
    /* This is a standard case (slerp). */
    float omega = acosf(cosom);
    float sinom = sinf(omega);
    scale0 = sinf((1.0f - amount) * omega) / sinom;
    scale1 = sinf(amount * omega) / sinom;
  } else {
    /* "from" and "to" quaternions are very close */
    /*  so we can do a linear interpolation.      */
    scale0 = 1.0f - amount;
    scale1 = amount;
  }

  return quat(
      (scale0 * from.x) + (scale1 * afto1[0]),
      (scale0 * from.y) + (scale1 * afto1[1]),
      (scale0 * from.z) + (scale1 * afto1[2]),
      (scale0 * from.w) + (scale1 * afto1[3]));

}

float quatDot(Quat q1, Quat q2)
{
  return q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;
}

Quat quatExp(Vec3 w)
{

  float theta = (float) sqrt(vec3Dot(w, w));
  float len = theta < LDK_EPSILON ? 1 : (float) (sin(theta) / theta);
  Vec3 v = vec3Mul(w, len);

  return quat(v.x, v.y, v.z, (float) cos(theta));
}

Vec3 quatLog(Quat q)
{
  float len = vec3Length(quatImaginaries(q));
  float angle = (float) atan2(len, q.w);
  len = len > LDK_EPSILON ? angle / len : 1;
  return vec3Mul(quatImaginaries(q), len);
}

static Quat quatGetValue(float t, Vec3 axis)
{
  return  quatExp(vec3Mul(axis, (float) (t / 2.0)) );
}

Quat quatConstrain(Quat q, Vec3 axis)
{

  const Quat orient = quat(0, 0, 0, 1);

  Vec3 vs = quatImaginaries(q);
  Vec3 v0 = quatImaginaries(orient);

  float a = q.w * orient.w + vec3Dot(vs, v0);
  float b = orient.w * vec3Dot(axis, vs) - q.w * vec3Dot(axis, v0) + vec3Dot(vs, vec3MulVec3(axis, v0));

  float alpha = (float) atan2(a, b);

  float t1 = (float) (-2 * alpha + M_PI);
  float t2 = (float) (-2 * alpha - M_PI);

  if ( quatDot(q, quatGetValue(t1, axis)) > 
      quatDot(q, quatGetValue(t2, axis)) )
  {
    return quatGetValue(t1, axis);
  }

  return quatGetValue(t2, axis);
}

Quat quatConstrainY(Quat q)
{
  return quatConstrain(q, vec3(0, 1, 0)); 
}

float quatDistance(Quat q0, Quat q1)
{
  Quat comb = quatMulQuat(quatInverse(q0), q1);
  return (float) sin(vec3Length(quatLog(comb)));
}

Quat quatNeg(Quat q)
{
  q.x = -q.x;
  q.y = -q.y;
  q.z = -q.z;
  q.w = -q.w;
  return q;
}

Quat quatScale(Quat q, float f)
{
  q.x = q.x * f;
  q.y = q.y * f;
  q.z = q.z * f;
  q.w = q.w * f;
  return q;
}

Quat quatInterpolate(Quat* qs, float* ws, int count)
{

  Quat ref = quatId();
  Quat refInv = quatInverse(ref);

  Vec3 acc = vec3Zero();

  for (int i = 0; i < count; i++ )
  {

    Vec3 qlog0 = quatLog(quatMulQuat(refInv, qs[i]));
    Vec3 qlog1 = quatLog(quatMulQuat(refInv, quatNeg(qs[i])));

    if (vec3Length(qlog0) < vec3Length(qlog1) )
    {
      acc = vec3Add(acc, vec3Mul(qlog0, ws[i]));
    } else {
      acc = vec3Add(acc, vec3Mul(qlog1, ws[i]));
    }
  }

  Quat res = quatMulQuat(ref, quatExp(acc));

  return quatNormalize(res);

}

Vec3 quatGetRight(Quat q)
{
  Vec3 right;
  right.x = 1 - 2 * (q.y * q.y + q.z * q.z);
  right.y = 2 * (q.x * q.y + q.w * q.z);
  right.z = 2 * (q.x * q.z - q.w * q.y);
  return right;
}

Vec3 quatGetForward(Quat q)
{
  Vec3 forward;
  forward.x = 2 * (q.x * q.z + q.w * q.y);
  forward.y = 2 * (q.y * q.z - q.w * q.x);
  forward.z = 1 - 2 * (q.x * q.x + q.y * q.y);
  return forward;
}

Vec3 quatGetUp(Quat q)
{
  Vec3 up;
  up.x = 2 * (q.x * q.y - q.w * q.z);
  up.y = 1 - 2 * (q.x * q.x + q.z * q.z);
  up.z = 2 * (q.y * q.z + q.w * q.x);
  return up;
}

QuatDual quatDual(Quat real, Quat dual)
{
  QuatDual qd;
  qd.real = real;
  qd.dual = dual;
  return qd;
}

QuatDual quatDualId(void)
{
  return quatDual(quatId(), vec4Zero());
}

QuatDual quatDualTransform(Quat q, Vec3 t)
{
  QuatDual qd;
  qd.real = q;
  qd.dual = quat(
      0.5f * ( t.x * q.w + t.y * q.z - t.z * q.y),
      0.5f * (-t.x * q.z + t.y * q.w + t.z * q.x),
      0.5f * ( t.x * q.y - t.y * q.x + t.z * q.w),
      -0.5f * ( t.x * q.x + t.y * q.y + t.z * q.z)
      );
  return qd;
}

QuatDual quatDualMul(QuatDual q0, QuatDual q1)
{
  return quatDual(
      quatMulQuat(q0.real, q1.real), 
      vec4Add(
        quatMulQuat(q0.real, q1.dual), 
        quatMulQuat(q0.dual, q1.real)));
}

QuatDual quatDualNormalize(QuatDual q)
{
  float l = quatLength(q.real);
  Quat real = vec4Mul(q.real, 1.0f / l);
  Quat dual = vec4Mul(q.dual, 1.0f / l);
  return quatDual(real, vec4Sub(dual, vec4Mul(real, quatDot(real, dual))));
}

Vec3 quatDualMulVec3(QuatDual q, Vec3 v)
{

  Vec3 rvc = vec3Cross(quatImaginaries(q.real), v);
  Vec3 real = vec3Cross(quatImaginaries(q.real), vec3Add(rvc, vec3Mul(v, q.real.w)));

  Vec3 rdc = vec3Cross(quatImaginaries(q.real), quatImaginaries(q.dual));  
  Vec3 rimg = vec3Mul(quatImaginaries(q.real), q.dual.w);
  Vec3 dimg = vec3Mul(quatImaginaries(q.dual), q.real.w);

  Vec3 dual = vec3Sub(rimg, vec3Add(dimg, rdc));

  return vec3Add(v, vec3Add(vec3Mul(real, 2), vec3Mul(dual, 2)));
}

Vec3 quatDualMulVec3Rot(QuatDual q, Vec3 v)
{

  Vec3 rvc = vec3Cross(quatImaginaries(q.real), v);
  Vec3 real = vec3Cross(quatImaginaries(q.real), vec3Add(rvc, vec3Mul(v, q.real.w)));

  return vec3Add(v, vec3Mul(real, 2.0));
}

#ifndef LDK_QUAT_FORWARD_VECTOR
  // Default forward vector (can be [1, 0, 0] or [0, 0, -1] depending on convention)
#define LDK_QUAT_FORWARD_VECTOR {-1.0f, 0.0f, 0.0f}
#endif  // LDK_QUAT_FORWARD_VECTOR

Quat quatFromDirection(Vec3 direction)
{
  Quat q = quatId();
  // Reference forward vector (default direction)
  Vec3 forward = LDK_QUAT_FORWARD_VECTOR;

  // Normalize the input direction vector
  direction = vec3Normalize(direction);

  // Compute the angle between the forward vector and the direction vector
  float dot_product = vec3Dot(forward, direction);
  float angle = acosf(dot_product);  // Angle in radians

  // Compute the axis of rotation (cross product of forward and direction)
  Vec3 axis = vec3Cross(forward, direction);

  // Normalize the axis of rotation
  axis = vec3Normalize(axis);

  // Compute the quaternion from the axis-angle representation
  float half_angle = angle / 2.0f;
  float sin_half_angle = sinf(half_angle);

  q.w = cosf(half_angle);
  q.x = axis.x * sin_half_angle;
  q.y = axis.y * sin_half_angle;
  q.z = axis.z * sin_half_angle;

  return q;
}

Vec3 quatToDirection(Quat quat)
{
  Vec3 forward = LDK_QUAT_FORWARD_VECTOR;
  Vec3 direction = quatMulVec3(quat, forward);
  return direction;
}

//
// Mat2
//

Mat2 mat2Id(void)
{
  Mat2 mat;
  mat.xx = 1.0f; mat.xy = 0.0f;
  mat.yx = 0.0f; mat.yy = 1.0f;
  return mat;
}

Mat2 mat2Zero(void)
{
  Mat2 mat;
  mat.xx = 0.0f; mat.xy = 0.0f;
  mat.yx = 0.0f; mat.yy = 0.0f;
  return mat;
}

Mat2 mat2(float xx, float xy, float yx, float yy)
{
  Mat2 mat;
  mat.xx = xx;
  mat.xy = xy;
  mat.yx = yx;
  mat.yy = yy;
  return mat;
}

Mat2 mat2MulMat2(Mat2 m1, Mat2 m2)
{
  Mat2 mat;

  mat.xx = m1.xx * m2.xx + m1.xy * m2.yx;
  mat.xy = m1.xx * m2.xy + m1.xy * m2.yy;
  mat.yx = m1.yx * m2.xx + m1.yy * m2.yx;
  mat.yy = m1.yx * m2.xy + m1.yy * m2.yy;

  return mat;
}

Vec2 mat2MulVec2(Mat2 m, Vec2 v)
{
  Vec2 vec;

  vec.x = v.x * m.xx + v.y * m.xy;
  vec.y = v.x * m.yx + v.y * m.yy;

  return vec;
}

Mat2 mat2Transpose(Mat2 m)
{
  Mat2 ret;
  ret.xx = m.xx;
  ret.xy = m.yx;
  ret.yx = m.xy;
  ret.yy = m.yy;
  return ret;
}

float mat2Det(Mat2 m)
{
  return m.xx * m.yy - m.xy * m.yx;
}

Mat2 mat2Inverse(Mat2 m)
{

  float det = mat2Det(m);
  float fac = (float) (1.0 / det);

  Mat2 ret;

  ret.xx = fac * m.yy;
  ret.xy = fac * -m.xy;
  ret.yx = fac * -m.yx;
  ret.yy = fac * m.xx;

  return ret;
}

void mat2ToArray(Mat2 m, float* out)
{

  out[0] = m.xx;
  out[1] = m.xy;
  out[2] = m.yx;
  out[3] = m.yy;

}

void mat2Print(Mat2 m)
{
  ldkLogInfo("|%4.2f, %4.2f|\n", m.xx, m.xy);
  ldkLogInfo("|%4.2f, %4.2f|\n", m.yx, m.yy);
}

Mat2 mat2Rotation(float a)
{
  Mat2 m;

  m.xx = (float) cos(a);
  m.xy = (float) -sin(a);
  m.yx = (float) sin(a);
  m.yy = (float) cos(a);

  return m;
}

//
// Mat3
//

Mat3 mat3Zero(void)
{
  Mat3 mat;

  mat.xx = 0.0f;
  mat.xy = 0.0f;
  mat.xz = 0.0f;

  mat.yx = 0.0f;
  mat.yy = 0.0f;
  mat.yz = 0.0f;

  mat.zx = 0.0f;
  mat.zy = 0.0f;
  mat.zz = 0.0f;

  return mat;
}

Mat3 mat3Id(void)
{
  Mat3 mat;

  mat.xx = 1.0f;
  mat.xy = 0.0f;
  mat.xz = 0.0f;

  mat.yx = 0.0f;
  mat.yy = 1.0f;
  mat.yz = 0.0f;

  mat.zx = 0.0f;
  mat.zy = 0.0f;
  mat.zz = 1.0f;

  return mat;
}

Mat3 mat3(float xx, float xy, float xz, float yx, float yy, float yz, float zx, float zy, float zz)
{
  Mat3 mat;

  mat.xx = xx;
  mat.xy = xy;
  mat.xz = xz;

  mat.yx = yx;
  mat.yy = yy;
  mat.yz = yz;

  mat.zx = zx;
  mat.zy = zy;
  mat.zz = zz;

  return mat;
}

Mat3 mat3MulMat3(Mat3 m1, Mat3 m2)
{
  Mat3 mat;

  mat.xx = (m1.xx * m2.xx) + (m1.xy * m2.yx) + (m1.xz * m2.zx);
  mat.xy = (m1.xx * m2.xy) + (m1.xy * m2.yy) + (m1.xz * m2.zy);
  mat.xz = (m1.xx * m2.xz) + (m1.xy * m2.yz) + (m1.xz * m2.zz);

  mat.yx = (m1.yx * m2.xx) + (m1.yy * m2.yx) + (m1.yz * m2.zx);
  mat.yy = (m1.yx * m2.xy) + (m1.yy * m2.yy) + (m1.yz * m2.zy);
  mat.yz = (m1.yx * m2.xz) + (m1.yy * m2.yz) + (m1.yz * m2.zz);

  mat.zx = (m1.zx * m2.xx) + (m1.zy * m2.yx) + (m1.zz * m2.zx);
  mat.zy = (m1.zx * m2.xy) + (m1.zy * m2.yy) + (m1.zz * m2.zy);
  mat.zz = (m1.zx * m2.xz) + (m1.zy * m2.yz) + (m1.zz * m2.zz);

  return mat;

}

Vec3 mat3MulVec3(Mat3 m, Vec3 v)
{

  Vec3 vec;

  vec.x = (m.xx * v.x) + (m.xy * v.y) + (m.xz * v.z);
  vec.y = (m.yx * v.x) + (m.yy * v.y) + (m.yz * v.z);
  vec.z = (m.zx * v.x) + (m.zy * v.y) + (m.zz * v.z);

  return vec;

}

Mat3 mat3Transpose(Mat3 m)
{
  Mat3 ret;
  ret.xx = m.xx;
  ret.xy = m.yx;
  ret.xz = m.zx;

  ret.yx = m.xy;
  ret.yy = m.yy;
  ret.yz = m.zy;

  ret.zx = m.xz;
  ret.zy = m.yz;
  ret.zz = m.zz;
  return ret;
}

float mat3Det(Mat3 m)
{
  return (m.xx * m.yy * m.zz) + (m.xy * m.yz * m.zx) + (m.xz * m.yx * m.zy) -
    (m.xz * m.yy * m.zx) - (m.xy * m.yx * m.zz) - (m.xx * m.yz * m.zy);
}

Mat3 mat3Inverse(Mat3 m)
{

  float det = mat3Det(m);
  float fac = (float) (1.0 / det);

  Mat3 ret;
  ret.xx = fac * mat2Det(mat2(m.yy, m.yz, m.zy, m.zz));
  ret.xy = fac * mat2Det(mat2(m.xz, m.xy, m.zz, m.zy));
  ret.xz = fac * mat2Det(mat2(m.xy, m.xz, m.yy, m.yz));

  ret.yx = fac * mat2Det(mat2(m.yz, m.yx, m.zz, m.zx));
  ret.yy = fac * mat2Det(mat2(m.xx, m.xz, m.zx, m.zz));
  ret.yz = fac * mat2Det(mat2(m.xz, m.xx, m.yz, m.yx));

  ret.zx = fac * mat2Det(mat2(m.yx, m.yy, m.zx, m.zy));
  ret.zy = fac * mat2Det(mat2(m.xy, m.xx, m.zy, m.zx));
  ret.zz = fac * mat2Det(mat2(m.xx, m.xy, m.yx, m.yy));

  return ret;

}

void mat3ToArray(Mat3 m, float* out)
{

  out[0] = m.xx;
  out[1] = m.yx;
  out[2] = m.zx;

  out[3] = m.xy;
  out[4] = m.yy;
  out[5] = m.zy;

  out[6] = m.xz;
  out[7] = m.yz;
  out[8] = m.zz;

}

void mat3Print(Mat3 m)
{
  ldkLogInfo("|%4.2f, %4.2f, %4.2f|\n", m.xx, m.xy, m.xz);
  ldkLogInfo("|%4.2f, %4.2f, %4.2f|\n", m.yx, m.yy, m.yz);
  ldkLogInfo("|%4.2f, %4.2f, %4.2f|\n", m.zx, m.zy, m.zz);
}

Mat3 mat3RotationX(float a)
{

  Mat3 m = mat3Id();

  m.yy = (float) cos(a);
  m.yz = (float) -sin(a);
  m.zy = (float) sin(a);
  m.zz = (float) cos(a);

  return m;

}

Mat3 mat3Scale(Vec3 s)
{

  Mat3 m = mat3Id();
  m.xx = s.x;
  m.yy = s.y;
  m.zz = s.z;  
  return m;

}

Mat3 mat3RotationY(float a)
{

  Mat3 m = mat3Id();

  m.xx = (float) cos(a);
  m.xz = (float) sin(a);
  m.zx = (float) -sin(a);
  m.zz = (float) cos(a);

  return m;

}

Mat3 mat3RotationZ(float a)
{

  Mat3 m = mat3Id();

  m.xx = (float) cos(a);
  m.xy = (float) -sin(a);
  m.yx = (float) sin(a);
  m.yy = (float) cos(a);

  return m;

}

Mat3 mat3RotationAngleAxis(float a, Vec3 v)
{

  Mat3 m;

  float c = (float) cos(a);
  float s = (float) sin(a);
  float nc = 1 - c;

  m.xx = v.x * v.x * nc + c;
  m.xy = v.x * v.y * nc - v.z * s;
  m.xz = v.x * v.z * nc + v.y * s;

  m.yx = v.y * v.x * nc + v.z * s;
  m.yy = v.y * v.y * nc + c;
  m.yz = v.y * v.z * nc - v.x * s;

  m.zx = v.z * v.x * nc - v.y * s;
  m.zy = v.z * v.y * nc + v.x * s;
  m.zz = v.z * v.z * nc + c;

  return m;
}

//
// Mat4
//

Mat4 mat4Zero(void)
{
  Mat4 mat;

  mat.xx = 0.0f;
  mat.xy = 0.0f;
  mat.xz = 0.0f;
  mat.xw = 0.0f;

  mat.yx = 0.0f;
  mat.yy = 0.0f;
  mat.yz = 0.0f;
  mat.yw = 0.0f;

  mat.zx = 0.0f;
  mat.zy = 0.0f;
  mat.zz = 0.0f;
  mat.zw = 0.0f;

  mat.wx = 0.0f;
  mat.wy = 0.0f;
  mat.wz = 0.0f;
  mat.ww = 0.0f;

  return mat;
}

Mat4 mat4Id(void)
{
  Mat4 mat = mat4Zero();

  mat.xx = 1.0f;
  mat.yy = 1.0f;
  mat.zz = 1.0f;
  mat.ww = 1.0f;
  return mat;
}

float mat4At(Mat4 m, int col, int row)
{
  float* arr = (float*)(&m);
  return arr[col + (row * 4)];
}

Mat4 mat4Set(Mat4 m, int col, int row, float v)
{
  float* arr = (float*)(&m);
  arr[col + (row * 4)] = v;
  return m;
}

Mat4 mat4(float xx, float xy, float xz, float xw,
    float yx, float yy, float yz, float yw,
    float zx, float zy, float zz, float zw,
    float wx, float wy, float wz, float ww)
{

  Mat4 mat;

  mat.xx = xx;
  mat.xy = xy;
  mat.xz = xz;
  mat.xw = xw;

  mat.yx = yx;
  mat.yy = yy;
  mat.yz = yz;
  mat.yw = yw;

  mat.zx = zx;
  mat.zy = zy;
  mat.zz = zz;
  mat.zw = zw;

  mat.wx = wx;
  mat.wy = wy;
  mat.wz = wz;
  mat.ww = ww;

  return mat;
}

Mat4 mat4Transpose(Mat4 m)
{
  Mat4 mat;

  mat.xx = m.xx;
  mat.xy = m.yx;
  mat.xz = m.zx;
  mat.xw = m.wx;

  mat.yx = m.xy;
  mat.yy = m.yy;
  mat.yz = m.zy;
  mat.yw = m.wy;

  mat.zx = m.xz;
  mat.zy = m.yz;
  mat.zz = m.zz;
  mat.zw = m.wz;

  mat.wx = m.xw;
  mat.wy = m.yw;
  mat.wz = m.zw;
  mat.ww = m.ww;

  return mat;
}

Mat4 mat3ToMat4(Mat3 m)
{

  Mat4 mat;

  mat.xx = m.xx;
  mat.xy = m.xy;
  mat.xz = m.xz;
  mat.xw = 0.0f;

  mat.yx = m.yx;
  mat.yy = m.yy;
  mat.yz = m.yz;
  mat.yw = 0.0f;

  mat.zx = m.zx;
  mat.zy = m.zy;
  mat.zz = m.zz;
  mat.zw = 0.0f;

  mat.wx = 0.0f;
  mat.wy = 0.0f;
  mat.wz = 0.0f;
  mat.ww = 1.0f;

  return mat;
}

Mat4 mat4MulMat4(Mat4 m1, Mat4 m2)
{

  Mat4 mat;

  mat.xx = (m1.xx * m2.xx) + (m1.xy * m2.yx) + (m1.xz * m2.zx) + (m1.xw * m2.wx);
  mat.xy = (m1.xx * m2.xy) + (m1.xy * m2.yy) + (m1.xz * m2.zy) + (m1.xw * m2.wy);
  mat.xz = (m1.xx * m2.xz) + (m1.xy * m2.yz) + (m1.xz * m2.zz) + (m1.xw * m2.wz);
  mat.xw = (m1.xx * m2.xw) + (m1.xy * m2.yw) + (m1.xz * m2.zw) + (m1.xw * m2.ww);

  mat.yx = (m1.yx * m2.xx) + (m1.yy * m2.yx) + (m1.yz * m2.zx) + (m1.yw * m2.wx);
  mat.yy = (m1.yx * m2.xy) + (m1.yy * m2.yy) + (m1.yz * m2.zy) + (m1.yw * m2.wy);
  mat.yz = (m1.yx * m2.xz) + (m1.yy * m2.yz) + (m1.yz * m2.zz) + (m1.yw * m2.wz);
  mat.yw = (m1.yx * m2.xw) + (m1.yy * m2.yw) + (m1.yz * m2.zw) + (m1.yw * m2.ww);

  mat.zx = (m1.zx * m2.xx) + (m1.zy * m2.yx) + (m1.zz * m2.zx) + (m1.zw * m2.wx);
  mat.zy = (m1.zx * m2.xy) + (m1.zy * m2.yy) + (m1.zz * m2.zy) + (m1.zw * m2.wy);
  mat.zz = (m1.zx * m2.xz) + (m1.zy * m2.yz) + (m1.zz * m2.zz) + (m1.zw * m2.wz);
  mat.zw = (m1.zx * m2.xw) + (m1.zy * m2.yw) + (m1.zz * m2.zw) + (m1.zw * m2.ww);

  mat.wx = (m1.wx * m2.xx) + (m1.wy * m2.yx) + (m1.wz * m2.zx) + (m1.ww * m2.wx);
  mat.wy = (m1.wx * m2.xy) + (m1.wy * m2.yy) + (m1.wz * m2.zy) + (m1.ww * m2.wy);
  mat.wz = (m1.wx * m2.xz) + (m1.wy * m2.yz) + (m1.wz * m2.zz) + (m1.ww * m2.wz);
  mat.ww = (m1.wx * m2.xw) + (m1.wy * m2.yw) + (m1.wz * m2.zw) + (m1.ww * m2.ww);

  return mat;

}

Vec4 mat4MulVec4(Mat4 m, Vec4 v)
{

  Vec4 vec;

  vec.x = (m.xx * v.x) + (m.xy * v.y) + (m.xz * v.z) + (m.xw * v.w);
  vec.y = (m.yx * v.x) + (m.yy * v.y) + (m.yz * v.z) + (m.yw * v.w);
  vec.z = (m.zx * v.x) + (m.zy * v.y) + (m.zz * v.z) + (m.zw * v.w);
  vec.w = (m.wx * v.x) + (m.wy * v.y) + (m.wz * v.z) + (m.ww * v.w);

  return vec;
}

Vec3 mat4MulVec3(Mat4 m, Vec3 v)
{

  Vec4 vHomo = vec4(v.x, v.y, v.z, 1);
  vHomo = mat4MulVec4(m, vHomo);

  vHomo = vec4Div(vHomo, vHomo.w);

  return vec3(vHomo.x, vHomo.y, vHomo.z);
}

Mat3 mat4ToMat3(Mat4 m)
{

  Mat3 mat;

  mat.xx = m.xx;
  mat.xy = m.xy;
  mat.xz = m.xz;

  mat.yx = m.yx;
  mat.yy = m.yy;
  mat.yz = m.yz;

  mat.zx = m.zx;
  mat.zy = m.zy;
  mat.zz = m.zz;

  return mat;

}

Quat mat4ToQuat(Mat4 m)
{
  float tr = m.xx + m.yy + m.zz;
  if (tr > 0.0f)
  {
    float s = sqrtf( tr + 1.0f );
    float w = s / 2.0f;
    float x = ( mat4At(m, 1, 2) - mat4At(m, 2, 1) ) * (0.5f / s);
    float y = ( mat4At(m, 2, 0) - mat4At(m, 0, 2) ) * (0.5f / s);
    float z = ( mat4At(m, 0, 1) - mat4At(m, 1, 0) ) * (0.5f / s);
    return quat(x, y, z, w);
  }
  else
  {
    int nxt[3] = {1, 2, 0};
    float q[4];
    int  i, j, k;
    i = 0;
    if ( mat4At(m, 1, 1) > mat4At(m, 0, 0) )
    {	i = 1;	}
    if ( mat4At(m, 2, 2) > mat4At(m, i, i) )
    {	i = 2;	}
    j = nxt[i];
    k = nxt[j];
    float s = sqrtf( (mat4At(m, i, i) - (mat4At(m, j, j) + mat4At(m, k, k))) + 1.0f );

    q[i] = s * 0.5f;

    if ( s != 0.0f )	{	s = 0.5f / s;	}

    q[3] = ( mat4At(m, j, k) - mat4At(m, k, j) ) * s;
    q[j] = ( mat4At(m, i, j) + mat4At(m, j, i) ) * s;
    q[k] = ( mat4At(m, i, k) + mat4At(m, k, i) ) * s;

    return quat(q[0], q[1], q[2], q[3]);
  }
}

QuatDual mat4ToQuatDual(Mat4 m)
{
  Quat rotation = mat4ToQuat(m);
  Vec3 translation = mat4MulVec3(m, vec3Zero());
  return quatDualTransform(rotation, translation);
}

float mat4Det(Mat4 m)
{
  float cofactXx =  mat3Det(mat3(m.yy, m.yz, m.yw, m.zy, m.zz, m.zw, m.wy, m.wz, m.ww));
  float cofactXy = -mat3Det(mat3(m.yx, m.yz, m.yw, m.zx, m.zz, m.zw, m.wx, m.wz, m.ww));
  float cofactXz =  mat3Det(mat3(m.yx, m.yy, m.yw, m.zx, m.zy, m.zw, m.wx, m.wy, m.ww));
  float cofactXw = -mat3Det(mat3(m.yx, m.yy, m.yz, m.zx, m.zy, m.zz, m.wx, m.wy, m.wz));

  return (cofactXx * m.xx) + (cofactXy * m.xy) + (cofactXz * m.xz) + (cofactXw * m.xw);
}

Mat4 mat4Inverse(Mat4 m)
{
  float det = mat4Det(m);
  if (det == 0.0f)
    return mat4Id();

  float fac = 1.0f / det;

  Mat4 ret;
  ret.xx = fac *  mat3Det(mat3(m.yy, m.yz, m.yw, m.zy, m.zz, m.zw, m.wy, m.wz, m.ww));
  ret.xy = fac * -mat3Det(mat3(m.yx, m.yz, m.yw, m.zx, m.zz, m.zw, m.wx, m.wz, m.ww));
  ret.xz = fac *  mat3Det(mat3(m.yx, m.yy, m.yw, m.zx, m.zy, m.zw, m.wx, m.wy, m.ww));
  ret.xw = fac * -mat3Det(mat3(m.yx, m.yy, m.yz, m.zx, m.zy, m.zz, m.wx, m.wy, m.wz));

  ret.yx = fac * -mat3Det(mat3(m.xy, m.xz, m.xw, m.zy, m.zz, m.zw, m.wy, m.wz, m.ww));
  ret.yy = fac *  mat3Det(mat3(m.xx, m.xz, m.xw, m.zx, m.zz, m.zw, m.wx, m.wz, m.ww));
  ret.yz = fac * -mat3Det(mat3(m.xx, m.xy, m.xw, m.zx, m.zy, m.zw, m.wx, m.wy, m.ww));
  ret.yw = fac *  mat3Det(mat3(m.xx, m.xy, m.xz, m.zx, m.zy, m.zz, m.wx, m.wy, m.wz));

  ret.zx = fac *  mat3Det(mat3(m.xy, m.xz, m.xw, m.yy, m.yz, m.yw, m.wy, m.wz, m.ww));
  ret.zy = fac * -mat3Det(mat3(m.xx, m.xz, m.xw, m.yx, m.yz, m.yw, m.wx, m.wz, m.ww));
  ret.zz = fac *  mat3Det(mat3(m.xx, m.xy, m.xw, m.yx, m.yy, m.yw, m.wx, m.wy, m.ww));
  ret.zw = fac * -mat3Det(mat3(m.xx, m.xy, m.xz, m.yx, m.yy, m.yz, m.wx, m.wy, m.wz));

  ret.wx = fac * -mat3Det(mat3(m.xy, m.xz, m.xw, m.yy, m.yz, m.yw, m.zy, m.zz, m.zw));
  ret.wy = fac *  mat3Det(mat3(m.xx, m.xz, m.xw, m.yx, m.yz, m.yw, m.zx, m.zz, m.zw));
  ret.wz = fac * -mat3Det(mat3(m.xx, m.xy, m.xw, m.yx, m.yy, m.yw, m.zx, m.zy, m.zw));
  ret.ww = fac *  mat3Det(mat3(m.xx, m.xy, m.xz, m.yx, m.yy, m.yz, m.zx, m.zy, m.zz));

  ret = mat4Transpose(ret);

  return ret;
}

void mat4ToArray(Mat4 m, float* out)
{

  out[0] = m.xx;
  out[1] = m.yx;
  out[2] = m.zx;
  out[3] = m.wx;

  out[4] = m.xy;
  out[5] = m.yy;
  out[6] = m.zy;
  out[7] = m.wy;

  out[8] = m.xz;
  out[9] = m.yz;
  out[10] = m.zz;
  out[11] = m.wz;

  out[12] = m.xw;
  out[13] = m.yw;
  out[14] = m.zw;
  out[15] = m.ww;

}

void mat4ToArrayTrans(Mat4 m, float* out)
{

  out[0] = m.xx;
  out[1] = m.xy;
  out[2] = m.xz;
  out[3] = m.xw;

  out[4] = m.yx;
  out[5] = m.yy;
  out[6] = m.yz;
  out[7] = m.yw;

  out[8] = m.zx;
  out[9] = m.zy;
  out[10] = m.zz;
  out[11] = m.zw;

  out[12] = m.wx;
  out[13] = m.wy;
  out[14] = m.wz;
  out[15] = m.ww;

}

void mat4Print(Mat4 m)
{
  ldkLogInfo("|%4.2f, %4.2f, %4.2f, %4.2f|\n", m.xx, m.xy, m.xz, m.xw);
  ldkLogInfo("|%4.2f, %4.2f, %4.2f, %4.2f|\n", m.yx, m.yy, m.yz, m.yw);
  ldkLogInfo("|%4.2f, %4.2f, %4.2f, %4.2f|\n", m.zx, m.zy, m.zz, m.zw);
  ldkLogInfo("|%4.2f, %4.2f, %4.2f, %4.2f|\n", m.wx, m.wy, m.wz, m.ww);
}

Mat4 mat4ViewLookAt(Vec3 position, Vec3 target, Vec3 up)
{
  // Calculating Local Coordinate System
  Vec3 zaxis = vec3Normalize(vec3Sub(target, position));
  Vec3 xaxis = vec3Normalize(vec3Cross(zaxis, up));
  Vec3 yaxis = vec3Cross(xaxis, zaxis);

  // Constructing the View Matrix
  Mat4 viewMatrix = mat4Id();
  viewMatrix.xx = xaxis.x;
  viewMatrix.xy = xaxis.y;
  viewMatrix.xz = xaxis.z;

  viewMatrix.yx = yaxis.x;
  viewMatrix.yy = yaxis.y;
  viewMatrix.yz = yaxis.z;

  viewMatrix.zx = -zaxis.x;
  viewMatrix.zy = -zaxis.y;
  viewMatrix.zz = -zaxis.z;

  // Translate to Camera Position
  // Multiply the view matrix by a translation matrix that moves the camera to its position.
  // The vec3Neg(position) is used to create a translation matrix that moves in the opposite direction of the camera position.
  viewMatrix = mat4MulMat4(viewMatrix, mat4Translation(vec3Neg(position)));
  return viewMatrix;
}

Mat4 mat4Perspective(float fov, float nearClip, float farClip, float ratio)
{
  float right, left, bottom, top;

  top = tanf(fov / 2.0f) * nearClip;
  bottom = -top;
  right = ratio * top;
  left = -right;

  Mat4 projMatrix = mat4Zero(); // just creates a zero-filled matrix4
  projMatrix.xx = (2.0f * nearClip) / (right - left);
  projMatrix.yy = (2.0f * nearClip) / (top - bottom);

  // Adjust the signs based on the handedness of the coordinate system
  projMatrix.xz = (right + left) / (right - left);
  projMatrix.yz = (top + bottom) / (top - bottom);

  // Adjust the signs for left-handed coordinate system
  projMatrix.zz = farClip / (nearClip - farClip);
  projMatrix.wz = -1.0f;
  projMatrix.zw = (nearClip * farClip) / (nearClip - farClip);

  //ldkLogInfo("left = %f, right = %f, top = %f, bottom = %f", left, right, top, bottom);
  return projMatrix;
}

Mat4 mat4Orthographic(float left, float right, float bottom, float top, float clipNear, float clipFar)
{
  Mat4 m = mat4Id();

  m.xx = 2 / (right - left);
  m.yy = 2 / (top - bottom);
  m.zz = 1 / (clipNear - clipFar);

  m.xw = -1 - 2 * left / (right - left);
  m.yw =  1 + 2 * top  / (bottom - top);
  m.zw = clipNear / (clipNear - clipFar);

  return m;
}

Mat4 mat4Translation(Vec3 v)
{
  Mat4 m = mat4Id();
  m.xw = v.x;
  m.yw = v.y;
  m.zw = v.z;

  return m;
}

Mat4 mat4Scale(Vec3 v)
{
  Mat4 m = mat4Id();
  m.xx = v.x;
  m.yy = v.y;
  m.zz = v.z;
  return m;
}

Mat4 mat4RotationX(float a)
{
  Mat4 m = mat4Id();
  m.yy = (float) cos(a);
  m.yz = (float) -sin(a);
  m.zy = (float) sin(a);
  m.zz = (float) cos(a);
  return m;
}

Mat4 mat4RotationY(float a)
{
  Mat4 m = mat4Id();
  m.xx = (float) cos(a);
  m.xz = (float) sin(a);
  m.zx = (float) -sin(a);
  m.zz = (float) cos(a);
  return m;
}

Mat4 mat4RotationZ(float a)
{
  Mat4 m = mat4Id();
  m.xx = (float) cos(a);
  m.xy = (float) -sin(a);
  m.yx = (float) sin(a);
  m.yy = (float) cos(a);
  return m;
}

Mat4 mat4RotationAxisAngle(Vec3 v, float angle)
{
  Mat4 m = mat4Id();
  float c =  (float) cos(angle);
  float s =  (float) sin(angle);
  float nc =  (1 - c);

  m.xx = v.x * v.x * nc + c;
  m.xy = v.x * v.y * nc - v.z * s;
  m.xz = v.x * v.z * nc + v.y * s;

  m.yx = v.y * v.x * nc + v.z * s;
  m.yy = v.y * v.y * nc + c;
  m.yz = v.y * v.z * nc - v.x * s;

  m.zx = v.z * v.x * nc - v.y * s;
  m.zy = v.z * v.y * nc + v.x * s;
  m.zz = v.z * v.z * nc + c;

  return m;
}

Mat4 mat4RotationEuler(float x, float y, float z)
{
  Mat4 m = mat4Zero();

  float cosx = (float) cos(x);
  float cosy = (float) cos(y);
  float cosz = (float) cos(z);
  float sinx = (float) sin(x);
  float siny = (float) sin(y);
  float sinz = (float) sin(z);

  m.xx = cosy * cosz;
  m.yx = -cosx * sinz + sinx * siny * cosz;
  m.zx = sinx * sinz + cosx * siny * cosz;

  m.xy = cosy * sinz;
  m.yy = cosx * cosz + sinx * siny * sinz;
  m.zy = -sinx * cosz + cosx * siny * sinz;

  m.xz = -siny;
  m.yz = sinx * cosy;
  m.zz = cosx * cosy;

  m.ww = 1;

  return m;
}

Mat4 mat4RotationQuat(Vec4 q)
{
  float x2 = q.x + q.x; 
  float y2 = q.y + q.y; 
  float z2 = q.z + q.z;
  float xx = q.x * x2;  
  float yy = q.y * y2;  
  float wx = q.w * x2;  
  float xy = q.x * y2;   
  float yz = q.y * z2;   
  float wy = q.w * y2;
  float xz = q.x * z2;
  float zz = q.z * z2;  
  float wz = q.w * z2;  

  return mat4(
      1.0f - ( yy + zz ),	xy - wz, xz + wy,	0.0f,
      xy + wz, 1.0f - ( xx + zz ), yz - wx, 0.0f,
      xz - wy, yz + wx, 1.0f - ( xx + yy ), 0.0f,
      0.0f,	0.0f, 0.0f,	1.0f);

}

Mat4 mat4RotationQuatDual(QuatDual q)
{
  float rx = q.real.x, ry = q.real.y, rz = q.real.z, rw = q.real.w;
  float tx = q.dual.x, ty = q.dual.y, tz = q.dual.z, tw = q.dual.w;

  Mat4 m = mat4Id();
  m.xx = rw*rw + rx*rx - ry*ry - rz*rz;              
  m.xy = 2.f*(rx*ry - rw*rz);                        
  m.xz = 2*(rx*rz + rw*ry);
  m.yx = 2*(rx*ry + rw*rz);                                  
  m.yy = rw*rw - rx*rx + ry*ry - rz*rz;      
  m.yz = 2*(ry*rz - rw*rx);
  m.zx = 2*(rx*rz - rw*ry);                                  
  m.zy = 2*(ry*rz + rw*rx);                          
  m.zz = rw*rw - rx*rx - ry*ry + rz*rz;

  m.xw = -2*tw*rx + 2*rw*tx - 2*ty*rz + 2*ry*tz;
  m.yw = -2*tw*ry + 2*tx*rz - 2*rx*tz + 2*rw*ty;
  m.zw = -2*tw*rz + 2*rx*ty + 2*rw*tz - 2*tx*ry;

  return m;
}

Mat4 mat4World(Vec3 position, Vec3 scale, Quat rotation)
{
  Mat4 posM, scaM, rotM, result;

  posM = mat4Translation(position);
  rotM = mat4RotationQuat(rotation);
  scaM = mat4Scale(scale);

  result = mat4Id();
  result = mat4MulMat4( result, posM );
  result = mat4MulMat4( result, rotM );
  result = mat4MulMat4( result, scaM );

  return result;

}

Mat4 mat4Lerp(Mat4 m1, Mat4 m2, float amount)
{
  Mat4 m;

  m.xx = lerp(m1.xx, m2.xx, amount);
  m.xy = lerp(m1.xy, m2.xy, amount);
  m.xz = lerp(m1.xz, m2.xz, amount);
  m.xw = lerp(m1.xw, m2.xw, amount);

  m.yx = lerp(m1.yx, m2.yx, amount);
  m.yy = lerp(m1.yy, m2.yy, amount);
  m.yz = lerp(m1.yz, m2.yz, amount);
  m.yw = lerp(m1.yw, m2.yw, amount);

  m.zx = lerp(m1.zx, m2.zx, amount);
  m.zy = lerp(m1.zy, m2.zy, amount);
  m.zz = lerp(m1.zz, m2.zz, amount);
  m.zw = lerp(m1.zw, m2.zw, amount);

  m.wx = lerp(m1.wx, m2.wx, amount);
  m.wy = lerp(m1.wy, m2.wy, amount);
  m.wz = lerp(m1.wz, m2.wz, amount);
  m.ww = lerp(m1.ww, m2.ww, amount);

  return m;
}

Mat4 mat4Smoothstep(Mat4 m1, Mat4 m2, float amount)
{
  Mat4 m;

  m.xx = (float) smoothstep(m1.xx, m2.xx, amount);
  m.xy = (float) smoothstep(m1.xy, m2.xy, amount);
  m.xz = (float) smoothstep(m1.xz, m2.xz, amount);
  m.xw = (float) smoothstep(m1.xw, m2.xw, amount);

  m.yx = (float) smoothstep(m1.yx, m2.yx, amount);
  m.yy = (float) smoothstep(m1.yy, m2.yy, amount);
  m.yz = (float) smoothstep(m1.yz, m2.yz, amount);
  m.yw = (float) smoothstep(m1.yw, m2.yw, amount);

  m.zx = (float) smoothstep(m1.zx, m2.zx, amount);
  m.zy = (float) smoothstep(m1.zy, m2.zy, amount);
  m.zz = (float) smoothstep(m1.zz, m2.zz, amount);
  m.zw = (float) smoothstep(m1.zw, m2.zw, amount);

  m.wx = (float) smoothstep(m1.wx, m2.wx, amount);
  m.wy = (float) smoothstep(m1.wy, m2.wy, amount);
  m.wz = (float) smoothstep(m1.wz, m2.wz, amount);
  m.ww = (float) smoothstep(m1.ww, m2.ww, amount);

  return m;
}

