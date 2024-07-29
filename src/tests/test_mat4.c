#include "../include/ldk/maths.h"
#define LDK_TEST_IMPLEMENTATION
#include "ldk_test.h"

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

int test_mat4Zero() {
  Mat4 zero = mat4Zero();
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      ASSERT_FLOAT_EQ(mat4At(zero, i, j), 0.0f);
    }
  }
  return 0;
}

int test_mat4() {
  Mat4 m = mat4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  ASSERT_FLOAT_EQ(mat4At(m, 0, 0), 1.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 0, 1), 2.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 0, 2), 3.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 0, 3), 4.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 1, 0), 5.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 1, 1), 6.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 1, 2), 7.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 1, 3), 8.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 2, 0), 9.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 2, 1), 10.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 2, 2), 11.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 2, 3), 12.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 3, 0), 13.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 3, 1), 14.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 3, 2), 15.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 3, 3), 16.0f);
  return 0;
}

int test_mat4Set() {
  Mat4 m = mat4Zero();
  m = mat4Set(m, 2, 3, 5.0f);
  ASSERT_FLOAT_EQ(mat4At(m, 2, 3), 5.0f);
  return 0;
}

int test_mat4Transpose() {
  Mat4 m = mat4(1, 2, 3, 4,
      5, 6, 7, 8,
      9, 10, 11, 12,
      13, 14, 15, 16);
  Mat4 t = mat4Transpose(m);
  ASSERT_FLOAT_EQ(mat4At(t, 0, 0), 1.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 1, 0), 2.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 2, 0), 3.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 3, 0), 4.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 0, 1), 5.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 1, 1), 6.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 2, 1), 7.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 3, 1), 8.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 0, 2), 9.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 1, 2), 10.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 2, 2), 11.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 3, 2), 12.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 0, 3), 13.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 1, 3), 14.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 2, 3), 15.0f);
  ASSERT_FLOAT_EQ(mat4At(t, 3, 3), 16.0f);
  return 0;
}

int test_mat4MulMat4() {
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
  ASSERT_FLOAT_EQ(mat4At(result, 0, 1), 260.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 0, 2), 270.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 0, 3), 280.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 1, 0), 618.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 1, 1), 644.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 1, 2), 670.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 1, 3), 696.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 2, 0), 986.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 2, 1), 1028.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 2, 2), 1070.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 2, 3), 1112.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 3, 0), 1354.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 3, 1), 1412.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 3, 2), 1470.0f);
  ASSERT_FLOAT_EQ(mat4At(result, 3, 3), 1528.0f);
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
  };

  return ldk_run_tests(tests, sizeof(tests)/sizeof(tests[0]));
}
