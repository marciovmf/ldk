/* Compilable input fixture for end-to-end Comet metadata generation. */
#ifndef LDK_COMET_TEST_FIXTURE_H
#define LDK_COMET_TEST_FIXTURE_H

#include <ldk_common.h>
#include <module/ldk_system.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_string.h>

//@enum
typedef enum TestCometMode
{
  TEST_COMET_MODE_OFF = 0,
  TEST_COMET_MODE_ON = 1
} TestCometMode;

typedef struct TestCometElement
{
  //@inspect headline
  XSmallstr name;
  float amount;
} TestCometElement;

X_ARRAY_TYPE(TestCometElement)

//@component
typedef struct TestCometComponent
{
  //@inspect slider min=0 max=100
  float speed;
  TestCometMode mode;
  //@inspect element_type=TestCometElement max_count=8
  XArray_TestCometElement *elements;
} TestCometComponent;

void test_comet_system_update(
    void *data, const LDKEntityGroup *group, float dt);

//@system name=TestCometSystem update=test_comet_system_update
typedef struct TestCometSystem
{
  float rate;
} TestCometSystem;

//@editor_callback component=TestCometComponent flags=Manual label=Refresh
void test_comet_editor_callback(LDKEntity entity, float dt);

#endif // LDK_COMET_TEST_FIXTURE_H
