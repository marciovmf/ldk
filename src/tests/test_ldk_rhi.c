#include <ldk_common.h>
#include <stdx/stdx_common.h>
#define X_IMPL_TEST
#include <stdx/stdx_test.h>

#include <module/ldk_rhi.h>


#include <stdlib.h>
#include <string.h>

typedef struct TestRHIBackend
{
  int shutdown_count;
  int begin_frame_count;
  int end_frame_count;
  int begin_pass_count;
  int end_pass_count;
  int bind_pipeline_count;
  int bind_bindings_count;
  int bind_vertex_buffer_count;
  int bind_index_buffer_count;
  int set_viewport_count;
  int set_scissor_count;
  int draw_count;
  int draw_indexed_count;
  int destroy_buffer_count;
  int destroy_pipeline_count;
  int destroy_bindings_count;
}
TestRHIBackend;

static void test_backend_shutdown(void* user_data)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  backend->shutdown_count++;
}

static void test_backend_begin_frame(void* user_data)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  backend->begin_frame_count++;
}

static void test_backend_end_frame(void* user_data)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  backend->end_frame_count++;
}

static void test_backend_begin_pass(void* user_data, const LDKRHIPassDesc* desc)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  (void)desc;
  backend->begin_pass_count++;
}

static void test_backend_end_pass(void* user_data)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  backend->end_pass_count++;
}

static void test_backend_bind_pipeline(void* user_data, LDKRHIPipeline pipeline)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  (void)pipeline;
  backend->bind_pipeline_count++;
}

static void test_backend_bind_bindings(void* user_data, LDKRHIBindings bindings)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  (void)bindings;
  backend->bind_bindings_count++;
}

static void test_backend_bind_vertex_buffer(void* user_data, LDKRHIBuffer buffer, uint32_t offset)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  (void)buffer;
  (void)offset;
  backend->bind_vertex_buffer_count++;
}

static void test_backend_bind_index_buffer(void* user_data, LDKRHIBuffer buffer, uint32_t offset, LDKRHIIndexType index_type)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  (void)buffer;
  (void)offset;
  (void)index_type;
  backend->bind_index_buffer_count++;
}

static void test_backend_set_viewport(void* user_data, const LDKRHIViewport* viewport)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  (void)viewport;
  backend->set_viewport_count++;
}

static void test_backend_set_scissor(void* user_data, const LDKRHIRect* scissor)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  (void)scissor;
  backend->set_scissor_count++;
}

static void test_backend_draw(void* user_data, const LDKRHIDrawDesc* desc)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  (void)desc;
  backend->draw_count++;
}

static void test_backend_draw_indexed(void* user_data, const LDKRHIDrawIndexedDesc* desc)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  (void)desc;
  backend->draw_indexed_count++;
}

static void test_backend_destroy_buffer(void* user_data, LDKRHIBuffer buffer)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  (void)buffer;
  backend->destroy_buffer_count++;
}

static void test_backend_destroy_pipeline(void* user_data, LDKRHIPipeline pipeline)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  (void)pipeline;
  backend->destroy_pipeline_count++;
}

static void test_backend_destroy_bindings(void* user_data, LDKRHIBindings bindings)
{
  TestRHIBackend* backend = (TestRHIBackend*)user_data;
  (void)bindings;
  backend->destroy_bindings_count++;
}

static LDKRHIFunctions test_rhi_functions(void)
{
  LDKRHIFunctions functions = {0};

  functions.shutdown = test_backend_shutdown;
  functions.destroy_buffer = test_backend_destroy_buffer;
  functions.destroy_pipeline = test_backend_destroy_pipeline;
  functions.destroy_bindings = test_backend_destroy_bindings;
  functions.begin_frame = test_backend_begin_frame;
  functions.end_frame = test_backend_end_frame;
  functions.begin_pass = test_backend_begin_pass;
  functions.end_pass = test_backend_end_pass;
  functions.bind_pipeline = test_backend_bind_pipeline;
  functions.bind_bindings = test_backend_bind_bindings;
  functions.bind_vertex_buffer = test_backend_bind_vertex_buffer;
  functions.bind_index_buffer = test_backend_bind_index_buffer;
  functions.set_viewport = test_backend_set_viewport;
  functions.set_scissor = test_backend_set_scissor;
  functions.draw = test_backend_draw;
  functions.draw_indexed = test_backend_draw_indexed;

  return functions;
}

static bool test_rhi_init(LDKRHIContext* rhi, TestRHIBackend* backend)
{
  LDKRHIContextDesc desc = {0};
  LDKRHIFunctions functions = test_rhi_functions();

  desc.backend_type = LDK_RHI_BACKEND_OPENGL33;
  desc.backend_user_data = backend;

  return ldk_rhi_initialize(rhi, &desc, &functions);
}

static LDKRHIPassDesc test_rhi_pass_desc(void)
{
  LDKRHIPassDesc desc;

  ldk_rhi_pass_desc_defaults(&desc);

  desc.color_attachment_count = 1;
  desc.color_attachments[0].texture = LDK_RHI_INVALID_TEXTURE;
  desc.color_attachments[0].load_op = LDK_RHI_LOAD_OP_CLEAR;
  desc.color_attachments[0].store_op = LDK_RHI_STORE_OP_STORE;

  return desc;
}

static void test_rhi_bind_complete_indexed_state(LDKRHIContext* rhi)
{
  ldk_rhi_bind_pipeline(rhi, 1);
  ldk_rhi_bind_bindings(rhi, 1);
  ldk_rhi_bind_vertex_buffer(rhi, 1, 0);
  ldk_rhi_bind_index_buffer(rhi, 2, 0, LDK_RHI_INDEX_TYPE_UINT32);
}

int test_rhi_pipeline_defaults_are_sane(void)
{
  LDKRHIPipelineDesc desc;

  ldk_rhi_pipeline_desc_defaults(&desc);

  ASSERT_TRUE(desc.topology == LDK_RHI_PRIMITIVE_TOPOLOGY_TRIANGLES);
  ASSERT_TRUE(desc.blend_state.color_write_mask == LDK_RHI_COLOR_WRITE_MASK_ALL);
  ASSERT_TRUE(desc.depth_state.test_enabled == true);
  ASSERT_TRUE(desc.depth_state.write_enabled == true);
  ASSERT_TRUE(desc.depth_state.compare_op == LDK_RHI_COMPARE_OP_LESS_EQUAL);
  ASSERT_TRUE(desc.raster_state.cull_mode == LDK_RHI_CULL_MODE_BACK);
  ASSERT_TRUE(desc.raster_state.front_face == LDK_RHI_FRONT_FACE_CCW);

  return 0;
}

int test_rhi_begin_pass_requires_active_frame(void)
{
  TestRHIBackend backend = {0};
  LDKRHIContext rhi = {0};
  bool initialized = test_rhi_init(&rhi, &backend);
  LDKRHIPassDesc pass = test_rhi_pass_desc();

  ASSERT_TRUE(initialized);

  ldk_rhi_begin_pass(&rhi, &pass);

  ASSERT_TRUE(backend.begin_pass_count == 0);

  ldk_rhi_terminate(&rhi);

  return 0;
}

int test_rhi_draw_indexed_requires_active_pass(void)
{
  TestRHIBackend backend = {0};
  LDKRHIContext rhi = {0};
  bool initialized = test_rhi_init(&rhi, &backend);
  LDKRHIDrawIndexedDesc draw = {0};

  ASSERT_TRUE(initialized);

  draw.index_count = 3;

  ldk_rhi_begin_frame(&rhi);
  test_rhi_bind_complete_indexed_state(&rhi);
  ldk_rhi_draw_indexed(&rhi, &draw);

  ASSERT_TRUE(backend.draw_indexed_count == 0);

  ldk_rhi_end_frame(&rhi);
  ldk_rhi_terminate(&rhi);

  return 0;
}

int test_rhi_draw_indexed_requires_pipeline(void)
{
  TestRHIBackend backend = {0};
  LDKRHIContext rhi = {0};
  bool initialized = test_rhi_init(&rhi, &backend);
  LDKRHIPassDesc pass = test_rhi_pass_desc();
  LDKRHIDrawIndexedDesc draw = {0};

  ASSERT_TRUE(initialized);

  draw.index_count = 3;

  ldk_rhi_begin_frame(&rhi);
  ldk_rhi_begin_pass(&rhi, &pass);
  ldk_rhi_bind_bindings(&rhi, 1);
  ldk_rhi_bind_vertex_buffer(&rhi, 1, 0);
  ldk_rhi_bind_index_buffer(&rhi, 2, 0, LDK_RHI_INDEX_TYPE_UINT32);
  ldk_rhi_draw_indexed(&rhi, &draw);

  ASSERT_TRUE(backend.draw_indexed_count == 0);

  ldk_rhi_end_pass(&rhi);
  ldk_rhi_end_frame(&rhi);
  ldk_rhi_terminate(&rhi);

  return 0;
}

int test_rhi_draw_indexed_requires_bindings(void)
{
  TestRHIBackend backend = {0};
  LDKRHIContext rhi = {0};
  bool initialized = test_rhi_init(&rhi, &backend);
  LDKRHIPassDesc pass = test_rhi_pass_desc();
  LDKRHIDrawIndexedDesc draw = {0};

  ASSERT_TRUE(initialized);

  draw.index_count = 3;

  ldk_rhi_begin_frame(&rhi);
  ldk_rhi_begin_pass(&rhi, &pass);
  ldk_rhi_bind_pipeline(&rhi, 1);
  ldk_rhi_bind_vertex_buffer(&rhi, 1, 0);
  ldk_rhi_bind_index_buffer(&rhi, 2, 0, LDK_RHI_INDEX_TYPE_UINT32);
  ldk_rhi_draw_indexed(&rhi, &draw);

  ASSERT_TRUE(backend.draw_indexed_count == 0);

  ldk_rhi_end_pass(&rhi);
  ldk_rhi_end_frame(&rhi);
  ldk_rhi_terminate(&rhi);

  return 0;
}

int test_rhi_draw_indexed_requires_index_buffer(void)
{
  TestRHIBackend backend = {0};
  LDKRHIContext rhi = {0};
  bool initialized = test_rhi_init(&rhi, &backend);
  LDKRHIPassDesc pass = test_rhi_pass_desc();
  LDKRHIDrawIndexedDesc draw = {0};

  ASSERT_TRUE(initialized);

  draw.index_count = 3;

  ldk_rhi_begin_frame(&rhi);
  ldk_rhi_begin_pass(&rhi, &pass);
  ldk_rhi_bind_pipeline(&rhi, 1);
  ldk_rhi_bind_bindings(&rhi, 1);
  ldk_rhi_bind_vertex_buffer(&rhi, 1, 0);
  ldk_rhi_draw_indexed(&rhi, &draw);

  ASSERT_TRUE(backend.draw_indexed_count == 0);

  ldk_rhi_end_pass(&rhi);
  ldk_rhi_end_frame(&rhi);
  ldk_rhi_terminate(&rhi);

  return 0;
}

int test_rhi_draw_indexed_calls_backend_when_state_is_complete(void)
{
  TestRHIBackend backend = {0};
  LDKRHIContext rhi = {0};
  bool initialized = test_rhi_init(&rhi, &backend);
  LDKRHIPassDesc pass = test_rhi_pass_desc();
  LDKRHIDrawIndexedDesc draw = {0};

  ASSERT_TRUE(initialized);

  draw.index_count = 3;

  ldk_rhi_begin_frame(&rhi);
  ldk_rhi_begin_pass(&rhi, &pass);
  test_rhi_bind_complete_indexed_state(&rhi);
  ldk_rhi_draw_indexed(&rhi, &draw);

  ASSERT_TRUE(backend.draw_indexed_count == 1);

  ldk_rhi_end_pass(&rhi);
  ldk_rhi_end_frame(&rhi);
  ldk_rhi_terminate(&rhi);

  return 0;
}

int test_rhi_pipeline_switch_invalidates_bindings(void)
{
  TestRHIBackend backend = {0};
  LDKRHIContext rhi = {0};
  bool initialized = test_rhi_init(&rhi, &backend);
  LDKRHIPassDesc pass = test_rhi_pass_desc();
  LDKRHIDrawIndexedDesc draw = {0};

  ASSERT_TRUE(initialized);

  draw.index_count = 3;

  ldk_rhi_begin_frame(&rhi);
  ldk_rhi_begin_pass(&rhi, &pass);
  test_rhi_bind_complete_indexed_state(&rhi);
  ldk_rhi_bind_pipeline(&rhi, 2);
  ldk_rhi_draw_indexed(&rhi, &draw);

  ASSERT_TRUE(backend.bind_pipeline_count == 2);
  ASSERT_TRUE(backend.draw_indexed_count == 0);

  ldk_rhi_bind_bindings(&rhi, 1);
  ldk_rhi_draw_indexed(&rhi, &draw);

  ASSERT_TRUE(backend.draw_indexed_count == 1);

  ldk_rhi_end_pass(&rhi);
  ldk_rhi_end_frame(&rhi);
  ldk_rhi_terminate(&rhi);

  return 0;
}

int test_rhi_viewport_and_scissor_require_active_pass(void)
{
  TestRHIBackend backend = {0};
  LDKRHIContext rhi = {0};
  bool initialized = test_rhi_init(&rhi, &backend);
  LDKRHIPassDesc pass = test_rhi_pass_desc();
  LDKRHIViewport viewport = {0};
  LDKRHIRect scissor = {0};

  ASSERT_TRUE(initialized);

  viewport.width = 640.0f;
  viewport.height = 480.0f;
  viewport.max_depth = 1.0f;
  scissor.width = 640;
  scissor.height = 480;

  ldk_rhi_begin_frame(&rhi);
  ldk_rhi_set_viewport(&rhi, &viewport);
  ldk_rhi_set_scissor(&rhi, &scissor);

  ASSERT_TRUE(backend.set_viewport_count == 0);
  ASSERT_TRUE(backend.set_scissor_count == 0);

  ldk_rhi_begin_pass(&rhi, &pass);
  ldk_rhi_set_viewport(&rhi, &viewport);
  ldk_rhi_set_scissor(&rhi, &scissor);

  ASSERT_TRUE(backend.set_viewport_count == 1);
  ASSERT_TRUE(backend.set_scissor_count == 1);

  ldk_rhi_end_pass(&rhi);
  ldk_rhi_end_frame(&rhi);
  ldk_rhi_terminate(&rhi);

  return 0;
}

int test_rhi_destroy_buffer_is_deferred(void)
{
  TestRHIBackend backend = {0};
  LDKRHIContext rhi = {0};
  bool initialized = test_rhi_init(&rhi, &backend);

  ASSERT_TRUE(initialized);

  ldk_rhi_destroy_buffer(&rhi, 1);

  ASSERT_TRUE(backend.destroy_buffer_count == 0);

  ldk_rhi_begin_frame(&rhi);
  ldk_rhi_end_frame(&rhi);
  ldk_rhi_begin_frame(&rhi);
  ldk_rhi_end_frame(&rhi);
  ldk_rhi_begin_frame(&rhi);
  ldk_rhi_end_frame(&rhi);

  ASSERT_TRUE(backend.destroy_buffer_count == 0);

  ldk_rhi_begin_frame(&rhi);

  ASSERT_TRUE(backend.destroy_buffer_count == 1);

  ldk_rhi_end_frame(&rhi);
  ldk_rhi_terminate(&rhi);

  return 0;
}

int test_rhi_pending_delete_cannot_be_bound(void)
{
  TestRHIBackend backend = {0};
  LDKRHIContext rhi = {0};
  bool initialized = test_rhi_init(&rhi, &backend);
  LDKRHIPassDesc pass = test_rhi_pass_desc();

  ASSERT_TRUE(initialized);

  ldk_rhi_destroy_buffer(&rhi, 1);

  ldk_rhi_begin_frame(&rhi);
  ldk_rhi_begin_pass(&rhi, &pass);
  ldk_rhi_bind_vertex_buffer(&rhi, 1, 0);

  ASSERT_TRUE(backend.bind_vertex_buffer_count == 0);

  ldk_rhi_end_pass(&rhi);
  ldk_rhi_end_frame(&rhi);
  ldk_rhi_terminate(&rhi);

  return 0;
}

int test_rhi_destroy_bound_pipeline_clears_bound_pipeline(void)
{
  TestRHIBackend backend = {0};
  LDKRHIContext rhi = {0};
  bool initialized = test_rhi_init(&rhi, &backend);
  LDKRHIPassDesc pass = test_rhi_pass_desc();
  LDKRHIDrawIndexedDesc draw = {0};

  ASSERT_TRUE(initialized);

  draw.index_count = 3;

  ldk_rhi_begin_frame(&rhi);
  ldk_rhi_begin_pass(&rhi, &pass);
  test_rhi_bind_complete_indexed_state(&rhi);

  ldk_rhi_destroy_pipeline(&rhi, 1);
  ldk_rhi_draw_indexed(&rhi, &draw);

  ASSERT_TRUE(backend.draw_indexed_count == 0);

  ldk_rhi_end_pass(&rhi);
  ldk_rhi_end_frame(&rhi);
  ldk_rhi_terminate(&rhi);

  return 0;
}

int test_rhi_destroy_bound_bindings_clears_bound_bindings(void)
{
  TestRHIBackend backend = {0};
  LDKRHIContext rhi = {0};
  bool initialized = test_rhi_init(&rhi, &backend);
  LDKRHIPassDesc pass = test_rhi_pass_desc();
  LDKRHIDrawIndexedDesc draw = {0};

  ASSERT_TRUE(initialized);

  draw.index_count = 3;

  ldk_rhi_begin_frame(&rhi);
  ldk_rhi_begin_pass(&rhi, &pass);
  test_rhi_bind_complete_indexed_state(&rhi);

  ldk_rhi_destroy_bindings(&rhi, 1);
  ldk_rhi_draw_indexed(&rhi, &draw);

  ASSERT_TRUE(backend.draw_indexed_count == 0);

  ldk_rhi_end_pass(&rhi);
  ldk_rhi_end_frame(&rhi);
  ldk_rhi_terminate(&rhi);

  return 0;
}

int main(void)
{
  STDXTestCase tests[] =
  {
    X_TEST(test_rhi_pipeline_defaults_are_sane),
    X_TEST(test_rhi_begin_pass_requires_active_frame),
    X_TEST(test_rhi_draw_indexed_requires_active_pass),
    X_TEST(test_rhi_draw_indexed_requires_pipeline),
    X_TEST(test_rhi_draw_indexed_requires_bindings),
    X_TEST(test_rhi_draw_indexed_requires_index_buffer),
    X_TEST(test_rhi_draw_indexed_calls_backend_when_state_is_complete),
    X_TEST(test_rhi_pipeline_switch_invalidates_bindings),
    X_TEST(test_rhi_viewport_and_scissor_require_active_pass),
    X_TEST(test_rhi_destroy_buffer_is_deferred),
    X_TEST(test_rhi_pending_delete_cannot_be_bound),
    X_TEST(test_rhi_destroy_bound_pipeline_clears_bound_pipeline),
    X_TEST(test_rhi_destroy_bound_bindings_clears_bound_bindings),
  };

  return x_tests_run(tests, sizeof(tests) / sizeof(tests[0]), NULL);
}
