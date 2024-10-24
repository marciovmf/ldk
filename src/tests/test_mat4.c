#include "../include/ldk/maths.h"
#include "common.h"
#define LDK_TEST_IMPLEMENTATION
#include "ldk_test.h"

#define mat4IsIdentity(m) do { \
  for (int i = 0; i < 4; ++i) { \
    for (int j = 0; j < 4; ++j) { \
      if (i == j) { \
        ASSERT_FLOAT_EQ(mat4At((m), i, j), 1.0f); } \
      else { \
        ASSERT_FLOAT_EQ(mat4At((m), i, j), 0.0f); } \
    } \
  }\
} while(0)

int test_mat4Id()
{
  Mat4 id = mat4Id();
  for (int i = 0; i < 4; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      if (i == j)
      {
        ASSERT_FLOAT_EQ(mat4At(id, i, j), 1.0f);
      }
      else
      {
        ASSERT_FLOAT_EQ(mat4At(id, i, j), 0.0f);
      }
    }
  }
  return 0;
}

int test_mat4Zero()
{
  Mat4 zero = mat4Zero();
  for (int i = 0; i < 4; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      ASSERT_FLOAT_EQ(mat4At(zero, i, j), 0.0f);
    }
  }
  return 0;
}

int test_mat4()
{
  Mat4 m = mat4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  ASSERT_FLOAT_EQ(mat4At(m, 0, 0), 1.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 1, 0), 2.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 2, 0), 3.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 3, 0), 4.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 0, 1), 5.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 1, 1), 6.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 2, 1), 7.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 3, 1), 8.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 0, 2), 9.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 1, 2), 10.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 2, 2), 11.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 3, 2), 12.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 0, 3), 13.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 1, 3), 14.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 2, 3), 15.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 3, 3), 16.0f);
  return 0;
}

int test_mat4Set()
{
  Mat4 m = mat4Zero();
  m = mat4Set(m, 2, 3, 5.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 2, 3), 5.0f);
  return 0;
}

int test_mat4Transpose()
{
  Mat4 m = mat4(1, 2, 3, 4,
      5, 6, 7, 8,
      9, 10, 11, 12,
      13, 14, 15, 16);
  Mat4 t = mat4Transpose(m);
  ASSERT_FLOAT_EQ(mat4At(t, 0, 0), 1.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 1, 0), 5.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 2, 0), 9.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 3, 0), 13.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 0, 1), 2.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 1, 1), 6.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 2, 1), 10.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 3, 1), 14.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 0, 2), 3.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 1, 2), 7.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 2, 2), 11.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 3, 2), 15.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 0, 3), 4.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 1, 3), 8.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 2, 3), 12.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 3, 3), 16.0f);
  return 0;
}

int test_mat4MulMat4()
{
  Mat4 m1 = mat4(1, 2, 3, 4,
      5, 6, 7, 8,
      9, 10, 11, 12,
      13, 14, 15, 16);
  Mat4 m2 = mat4(17, 18, 19, 20,
      21, 22, 23, 24,
      25, 26, 27, 28,
      29, 30, 31, 32);
  Mat4 result = mat4MulMat4(m1, m2);
  ASSERT_FLOAT_EQ(mat4At(result, 0, 0), 250.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 1, 0), 260.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 2, 0), 270.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 3, 0), 280.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 0, 1), 618.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 1, 1), 644.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 2, 1), 670.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 3, 1), 696.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 0, 2), 986.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 1, 2), 1028.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 2, 2), 1070.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 3, 2), 1112.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 0, 3), 1354.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 1, 3), 1412.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 2, 3), 1470.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 3, 3), 1528.0f);
  return 0;
}

int test_mat4Determinant()
{
  Mat4 m = mat4(
      4 ,0 ,1 ,2,
      7 ,5 ,0 ,6,
      2 ,0 ,3 ,2,
      3 ,4 ,2 ,1);

  float det = mat4Det(m);
  ASSERT_FLOAT_EQ(det, -178.0f);
  return 0;
}

int test_mat4Inverse()
{
  Mat4 m = mat4Id();
  m = mat4Inverse(m);
  // Ensure inverse of Identity is Identity
  for (int i = 0; i < 4; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      if (i == j)
      {
        ASSERT_FLOAT_EQ(mat4At(m, i, j), 1.0f);
      }
      else
      {
        ASSERT_FLOAT_EQ(mat4At(m, i, j), 0.0f);
      }
    }
  }

  // 
  // Check M * M ^−1 = I
  //
  m = mat4(
      4 ,0 ,1 ,2,
      7 ,5 ,0 ,6,
      2 ,0 ,3 ,2,
      3 ,4 ,2 ,1);
  Mat4 mi = mat4Inverse(m);
  Mat4 result = mat4MulMat4(m, mi);
  mat4IsIdentity(result);

  //
  //Check 𝑀 ^−1 * 𝑀 = I
  //
  result = mat4MulMat4(mi, m);
  mat4IsIdentity(result);

  //
  //Check (M ^ −1) ^ −1 = M
  //
  result = mat4Inverse(mi);
  ASSERT_FLOAT_EQ(mat4At(result, 0, 0), 4.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 1, 0), 0.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 2, 0), 1.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 3, 0), 2.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 0, 1), 7.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 1, 1), 5.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 2, 1), 0.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 3, 1), 6.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 0, 2), 2.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 1, 2), 0.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 2, 2), 3.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 3, 2), 2.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 0, 3), 3.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 1, 3), 4.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 2, 3), 2.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 3, 3), 1.0f);
  return 0;
}

int main()
{
  LDKTestCase tests[] =
  {
    TEST_CASE(test_mat4Id),
    TEST_CASE(test_mat4Zero),
    TEST_CASE(test_mat4),
    TEST_CASE(test_mat4Set),
    TEST_CASE(test_mat4Transpose),
    TEST_CASE(test_mat4MulMat4),
    TEST_CASE(test_mat4Determinant),
    TEST_CASE(test_mat4Inverse)
  };

  return ldk_run_tests(tests, sizeof(tests)/sizeof(tests[0]));
}
