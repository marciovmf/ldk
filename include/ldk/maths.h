/**
 *
 * maths.h
 * 
 * Common math functions and types for Floats Vector Quaternions and Matrices
 * 
 */

#ifndef LDK_MATHS_H
#define LDK_MATHS_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

  //
  // Float/double operations
  //

  LDK_API float clamp(float x, float bottom, float top);
  LDK_API bool  between(float x, float bottom, float top);
  LDK_API float lerp(float p1, float p2, float amount);
  LDK_API float smoothstep(float p1, float p2, float amount);
  LDK_API double degToRadian(double deg);
  LDK_API double radianToDeg(double radian);

  LDK_API int32 floatsAreEqualEpsilon(float a, float b, float epsilon);
  LDK_API int32 floatIsZeroEpsilon(float a, float epsilon);
  LDK_API int32 floatIsNegativeEpsilon(float a, float epsilon);
  LDK_API int32 floatIsPositiveEpsilon(float a, float epsilon);
  LDK_API int32 floatIsGreaterThanEpsilon(float a, float b, float epsilon);
  LDK_API int32 floatIsLessThanEpsilon(float a, float b, float epsilon);
  LDK_API int32 floatIsGreaterThanOrEqualEpsilon(float a, float b, float epsilon);
  LDK_API int32 floatIsLessThanOrEqualEpsilon(float a, float b, float epsilon);
  LDK_API int32 floatsAreAlmostEqualRelativeEpsilon(float a, float b, float relativeEpsilon);
  LDK_API bool  floatIsMultipleEpsilon(float a, float b, float epsilon);



#ifndef LDK_EPSILON
#define LDK_EPSILON 1e-9f
#endif //LDK_EPSILON

#define floatsAreEqual(a, b) floatsAreEqualEpsilon((a), (b), LDK_EPSILON)
#define floatIsZero(a) floatIsZeroEpsilon((a), LDK_EPSILON)
#define floatIsNegative(a) floatIsNegativeEpsilon((a), LDK_EPSILON)
#define floatIsPositive(a) floatIsPositiveEpsilon((a), LDK_EPSILON)
#define floatIsGreaterThan(a, b) floatIsGreaterThanEpsilon((a), (b), LDK_EPSILON)
#define floatIsLessThan(a, b) floatIsLessThanEpsilon((a), (b), LDK_EPSILON)
#define floatIsGreaterThanOrEqual(a, b) floatIsGreaterThanOrEqualEpsilon((a), (b), LDK_EPSILON)
#define floatIsLessThanOrEqual(a, b) floatIsLessThanOrEqualEpsilon((a), (b), LDK_EPSILON)
#define floatsAreAlmostEqualRelative(a, b) floatsAreAlmostEqualRelativeEpsilon((a), (b), LDK_EPSILON)
#define floatIsMultipleOf(a, b) floatIsMultipleEpsilon(a, b, LDK_EPSILON)


  //
  // Vec2
  //

  typedef struct 
  {
    float x;
    float y;
  } Vec2;

  LDK_API Vec2  vec2(float x, float y);
  LDK_API Vec2  vec2Zero(void);
  LDK_API Vec2  vec2One(void);
  LDK_API Vec2  vec2Add(Vec2 v1, Vec2 v2);
  LDK_API Vec2  vec2Sub(Vec2 v1, Vec2 v2);
  LDK_API Vec2  vec2Mul(Vec2 v, float fac);
  LDK_API Vec2  vec2MulVec2(Vec2 v1, Vec2 v2);
  LDK_API Vec2  vec2Pow(Vec2 v, float exp);
  LDK_API Vec2  vec2Div(Vec2 v, float fac);
  LDK_API Vec2  vec2DivVec2(Vec2 v1, Vec2 v2);
  LDK_API Vec2  vec2Pow(Vec2 v, float exp);
  LDK_API Vec2  vec2Neg(Vec2 v);
  LDK_API Vec2  vec2Abs(Vec2 v);
  LDK_API Vec2  vec2Floor(Vec2 v);
  LDK_API Vec2  vec2Max(Vec2 v, float x);
  LDK_API Vec2  vec2Min(Vec2 v, float x);
  LDK_API Vec2  vec2Clamp(Vec2 v, float b, float t);
  LDK_API void  vec2Print(Vec2 v);
  LDK_API bool  vec2Equ(Vec2 v1, Vec2 v2);
  LDK_API float vec2Dot(Vec2 v1, Vec2 v2);
  LDK_API float vec2LengthSqrd(Vec2 v);
  LDK_API float vec2Length(Vec2 v);
  LDK_API float vec2DistSqrd(Vec2 v1, Vec2 v2);
  LDK_API float vec2DistManhattan(Vec2 v1, Vec2 v2);
  LDK_API float vec2Dist(Vec2 v1, Vec2 v2);
  LDK_API Vec2  vec2Normalize(Vec2 v);
  LDK_API Vec2  vec2Reflect(Vec2 v1, Vec2 v2);
  LDK_API Vec2  vec2Fmod(Vec2 v, float val);
  LDK_API Vec2  vec2Lerp(Vec2 v1, Vec2 v2, float amount);
  LDK_API Vec2  vec2Smoothstep(Vec2 v1, Vec2 v2, float amount);

  //
  // Vec3
  //

  typedef struct 
  {
    float x;
    float y;
    float z;
  } Vec3;

  LDK_API Vec3 vec3(float x, float y, float z);
  LDK_API Vec3 vec3Zero(void);
  LDK_API Vec3 vec3One(void);
  LDK_API Vec3 vec3Up(void);
  LDK_API Vec3 vec3Right(void);
  LDK_API Vec3 vec3Forward(void);
  LDK_API Vec3 vec3Add(Vec3 v1, Vec3 v2);
  LDK_API Vec3 vec3Sub(Vec3 v1, Vec3 v2);
  LDK_API Vec3 vec3Mul(Vec3 v, float fac);
  LDK_API Vec3 vec3MulVec3(Vec3 v1, Vec3 v2);
  LDK_API Vec3 vec3Div(Vec3 v, float fac);
  LDK_API Vec3 vec3DivVec3(Vec3 v1, Vec3 v2);
  LDK_API Vec3 vec3Pow(Vec3 v, float fac);
  LDK_API Vec3 vec3Neg(Vec3 v);
  LDK_API Vec3 vec3Abs(Vec3 v);
  LDK_API Vec3 vec3Floor(Vec3 v);
  LDK_API Vec3 vec3Fmod(Vec3 v, float val);
  LDK_API void vec3Print(Vec3 v);
  LDK_API bool vec3Equ(Vec3 v1, Vec3 v2);
  LDK_API bool vec3Neq(Vec3 v1, Vec3 v2);
  LDK_API float vec3Dot(Vec3 v1, Vec3 v2);
  LDK_API float vec3LengthSqrd(Vec3 v);
  LDK_API float vec3Length(Vec3 v);
  LDK_API float vec3DistSqrd(Vec3 v1, Vec3 v2);
  LDK_API float vec3Dist(Vec3 v1, Vec3 v2);
  LDK_API float vec3DistManhattan(Vec3 v1, Vec3 v2);
  LDK_API Vec3 vec3Cross(Vec3 v1, Vec3 v2);
  LDK_API Vec3 vec3Normalize(Vec3 v);
  LDK_API Vec3 vec3Reflect(Vec3 v1, Vec3 v2);
  LDK_API Vec3 vec3Project(Vec3 v1, Vec3 v2);
  LDK_API Vec3 vec3Lerp(Vec3 v1, Vec3 v2, float amount);
  LDK_API Vec3 vec3Smoothstep(Vec3 v1, Vec3 v2, float amount);
  LDK_API Vec3 rgbToVec3(LDKRGB color);
  LDK_API LDKRGB vec3ToRGB(Vec3 v);

  /**
   * Calculates the unsigned angle between two 3D vectors, v1 and v2.
   *
   * @param v1 The first 3D vector.
   * @param v2 The second 3D vector.
   *
   * @return The angle in radians between the vectors v1 and v2, with a range of [0, π].
   *         The angle is calculated using the dot product, and it is always a positive,
   *         unsigned value. If either v1 or v2 has zero magnitude, the function returns 0.
   */
  LDK_API float vec3AngleBetween(Vec3 v1, Vec3 v2);

  /**
   * Calculates the signed angle between two 3D vectors, v1 and v2.
   *
   * @param v1 The first 3D vector.
   * @param v2 The second 3D vector.
   * @param normalDirection A 3D vector representing the reference normal direction.
   *        This vector is used to determine the sign of the angle. The sign is positive
   *        if the cross product of v1 and v2 points in the same direction as normalDirection,
   *        and negative if it points in the opposite direction.
   *
   * @return The signed angle in radians between the vectors v1 and v2.
   *         The angle will be positive or negative depending on the orientation of
   *         the vectors relative to normalDirection.
   *         If either v1 or v2 has zero magnitude, the function returns 0.
   */
  LDK_API float vec3SignedAngleBetween(Vec3 v1, Vec3 v2, Vec3 normalDirection);

  //
  // Vec4
  //

  typedef struct 
  {
    float x;
    float y;
    float z;
    float w;
  } Vec4;

  LDK_API Vec4 vec4(float x, float y, float z, float w);
  LDK_API Vec4 vec4Zero(void);
  LDK_API Vec4 vec4One(void);
  LDK_API Vec4 vec4Add(Vec4 v1, Vec4 v2);
  LDK_API Vec4 vec4Sub(Vec4 v1, Vec4 v2);
  LDK_API Vec4 vec4Mul(Vec4 v, float fac);
  LDK_API Vec4 vec4MulVec4(Vec4 v1, Vec4 v2);
  LDK_API Vec4 vec4Div(Vec4 v, float fac);
  LDK_API Vec4 vec4Pow(Vec4 v, float fac);
  LDK_API Vec4 vec4Neg(Vec4 v);
  LDK_API Vec4 vec4Abs(Vec4 v);
  LDK_API Vec4 vec4Floor(Vec4 v);
  LDK_API Vec4 vec4Fmod(Vec4 v, float val);
  LDK_API Vec4 vec4Sqrt(Vec4 v);
  LDK_API Vec4 vec4Max(Vec4 v1, Vec4 v2);
  LDK_API Vec4 vec4Min(Vec4 v1, Vec4 v2);
  LDK_API bool vec4Equ(Vec4 v1, Vec4 v2);
  LDK_API float vec4Dot(Vec4 v1, Vec4 v2);
  LDK_API float vec4LengthSqrd(Vec4 v);
  LDK_API float vec4Length(Vec4 v);
  LDK_API float vec4DistSqrd(Vec4 v1, Vec4 v2);
  LDK_API float vec4Dist(Vec4 v1, Vec4 v2);
  LDK_API float vec4DistManhattan(Vec4 v1, Vec4 v2);
  LDK_API Vec4 vec4Normalize(Vec4 v);
  LDK_API Vec4 vec4Reflect(Vec4 v1, Vec4 v2);
  LDK_API void vec4Print(Vec4 v);
  LDK_API Vec4 vec3ToHomogeneous(Vec3 v);
  LDK_API Vec3 vec4FromHomogeneous(Vec4 v);
  LDK_API Vec4 vec4Lerp(Vec4 v1, Vec4 v2, float amount);
  LDK_API Vec4 vec4Smoothstep(Vec4 v1, Vec4 v2, float amount);
  LDK_API Vec4 rgbaToVec4(LDKRGBA color);
  LDK_API Vec4 rgbToVec4(LDKRGB color);


  //
  // Quaternion
  //

  typedef Vec4 Quat;

  LDK_API Quat quatId(void);
  LDK_API Quat quat(float x, float y, float z, float w);
  LDK_API Quat quatFromEuler(Vec3 r);
  LDK_API Quat quatAngleAxis(float angle, Vec3 axis);
  LDK_API Quat quatRotationX(float angle);
  LDK_API Quat quatRotationY(float angle);
  LDK_API Quat quatRotationZ(float angle);
  LDK_API float quatAt(Quat q, int i);
  LDK_API float quatReal(Quat q);
  LDK_API Vec3 quatImaginaries(Quat q);
  LDK_API void quatToAngleAxis(Quat q, Vec3* axis, float* angle);
  LDK_API Vec3 quatToEuler(Quat q);
  LDK_API Quat quatNeg(Quat q);
  LDK_API float quatDot(Quat q1, Quat q2);
  LDK_API Quat quatScale(Quat q, float f);
  LDK_API Quat quatMulQuat(Quat q1, Quat q2);
  LDK_API Vec3 quatMulVec3(Quat q, Vec3 v);
  LDK_API Quat quatInverse(Quat q);
  LDK_API Quat quatUnitInverse(Quat q);
  LDK_API float quatLength(Quat q);
  LDK_API Quat quatNormalize(Quat q);
  LDK_API Quat quatExp(Vec3 w);
  LDK_API Vec3 quatLog(Quat q);
  LDK_API Quat quatSlerp(Quat q1, Quat q2, float amount);
  LDK_API Quat quatConstrain(Quat q, Vec3 axis);
  LDK_API Quat quatConstrainY(Quat q);
  LDK_API float quatDistance(Quat q0, Quat q1);
  LDK_API Quat quatInterpolate(Quat* qs, float* ws, int count);
  LDK_API Vec3 quatGetRight(Quat q);
  LDK_API Vec3 quatGetForward(Quat q);
  LDK_API Vec3 quatGetUp(Quat q);
  LDK_API Quat quatFromDirection(Vec3 direction);
  LDK_API Vec3 quatToDirection(Quat quat);


  typedef struct {
    Quat real;
    Quat dual;
  } QuatDual;

  LDK_API QuatDual quatDualId(void);
  LDK_API QuatDual quatDual(Quat real, Quat dual);
  LDK_API QuatDual quatDualTransform(Quat q, Vec3 t);
  LDK_API QuatDual quatDualMul(QuatDual q0, QuatDual q1);
  LDK_API Vec3 quatDualMulVec3(QuatDual q, Vec3 v);
  LDK_API Vec3 quatDualMulVec3Rot(QuatDual q, Vec3 v);

  //
  // Mat2
  //

  typedef struct {
    float xx; float xy;
    float yx; float yy;
  } Mat2;

  LDK_API Mat2 mat2Id(void);
  LDK_API Mat2 mat2Zero(void);
  LDK_API Mat2 mat2New(float xx, float xy, float yx, float yy);
  LDK_API Mat2 mat2MulMat2(Mat2 m1, Mat2 mat2);
  LDK_API Vec2 mat2MulVec2(Mat2 m, Vec2 v);
  LDK_API Mat2 mat2Transpose(Mat2 m);
  LDK_API float mat2Det(Mat2 m);
  LDK_API Mat2 mat2Inverse(Mat2 m);
  LDK_API void mat2ToArray(Mat2 m, float* out);
  LDK_API void mat2Print(Mat2 m);
  LDK_API Mat2 mat2Rotation(float a);

  //
  // Mat3
  //

  typedef struct {
    float xx; float xy; float xz;
    float yx; float yy; float yz;
    float zx; float zy; float zz;
  } Mat3;

  LDK_API Mat3 mat3Id(void);
  LDK_API Mat3 mat3Zero(void);
  LDK_API Mat3 mat3(float xx, float xy, float xz, float yx, float yy, float yz, float zx, float zy, float zz);
  LDK_API Mat3 mat3MulMat3(Mat3 m1, Mat3 mat2);
  LDK_API Vec3 mat3MulVec3(Mat3 m, Vec3 v);
  LDK_API Mat3 mat3Transpose(Mat3 m);
  LDK_API float mat3Det(Mat3 m);
  LDK_API Mat3 mat3Inverse(Mat3 m);
  LDK_API void mat3ToArray(Mat3 m, float* out);
  LDK_API void mat3Print(Mat3 m);
  LDK_API Mat3 mat3Scale(Vec3 s);
  LDK_API Mat3 mat3RotationX(float a);
  LDK_API Mat3 mat3RotationY(float a);
  LDK_API Mat3 mat3RotationZ(float a);
  LDK_API Mat3 mat3RotationAngleAxis(float angle, Vec3 axis);


  //
  // Mat4 (Column major)
  //

  typedef struct {
    float xx; float xy; float xz; float xw;
    float yx; float yy; float yz; float yw;
    float zx; float zy; float zz; float zw;
    float wx; float wy; float wz; float ww;
  } Mat4;

  LDK_API Mat4 mat4Id(void);
  LDK_API Mat4 mat4Zero(void);
  LDK_API Mat4 mat4(float xx, float xy, float xz, float xw,
      float yx, float yy, float yz, float yw,
      float zx, float zy, float zz, float zw,
      float wx, float wy, float wz, float ww);
  LDK_API float mat4At(Mat4 m, int col, int row);
  LDK_API Mat4 mat4Set(Mat4 m, int col, int row, float v);
  LDK_API Mat4 mat4Transpose(Mat4 m);
  LDK_API Mat4 mat4MulMat4(Mat4 m1, Mat4 mat2);
  LDK_API Vec4 mat4MulVec4(Mat4 m, Vec4 v);
  LDK_API Vec3 mat4MulVec3(Mat4 m, Vec3 v);
  LDK_API float mat4Det(Mat4 m);
  LDK_API Mat4 mat4Inverse(Mat4 m);
  LDK_API Mat4 mat3ToMat4(Mat3 m);
  LDK_API Mat3 mat4ToMat3(Mat4 m);
  LDK_API Quat mat4ToQuat(Mat4 m);
  LDK_API QuatDual mat4ToQuatDual(Mat4 m);
  LDK_API void mat4ToArray(Mat4 m, float* out);
  LDK_API void mat4ToArrayTrans(Mat4 m, float* out);
  LDK_API void mat4Print(Mat4 m);
  LDK_API Mat4 mat4Translation(Vec3 v);
  LDK_API Mat4 mat4Scale(Vec3 v);
  LDK_API Mat4 mat4RotationX(float a);
  LDK_API Mat4 mat4RotationY(float a);
  LDK_API Mat4 mat4RotationZ(float a);
  LDK_API Mat4 mat4RotationAxisAngle(Vec3 axis, float angle);
  LDK_API Mat4 mat4RotationEuler(float x, float y, float z);
  LDK_API Mat4 mat4RotationQuat(Quat q);
  LDK_API Mat4 mat4RotationQuatDual(QuatDual q);
  LDK_API Mat4 mat4ViewLookAt(Vec3 position, Vec3 target, Vec3 up);
  LDK_API Mat4 mat4Perspective(float fov, float nearClip, float farClip, float ratio);
  LDK_API Mat4 mat4Orthographic(float left, float right, float bottom, float top, float near, float far);
  LDK_API Mat4 mat4World(Vec3 position, Vec3 scale, Quat rotation);
  LDK_API Mat4 mat4Lerp(Mat4 m1, Mat4 mat2, float amount);
  LDK_API Mat4 mat4Smoothstep(Mat4 m1, Mat4 mat2, float amount);

  //
  // Bounds
  //

  typedef struct
  {
    Vec3 min;
    Vec3 max;
  }LDKAABB;

  typedef struct
  {
    Vec3 center;
    float radius;
  }LDKBoundingSphere;


#ifdef __cplusplus
}
#endif
#endif // LDK_MATHS_H

