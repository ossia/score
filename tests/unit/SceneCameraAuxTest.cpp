// UNIT — the Camera auxiliary buffer's declared byte range (#163).
//
// ISOLATED and EXPECTED RED, in the style of test_gfx_isf_findings: the
// assertion below fails on today's engine and that failure is the point.
//
// packAndUploadCameras packs one CameraUBOData per camera the flattener
// collected into m_camerasBuffer (capacity >= 16 of them) and puts the active
// camera at slot 0. Both publication sites in ScenePreprocessorNode then
// declare the `camera` auxiliary's byte_size through cameraAuxByteSize(),
// which ignores the count and always advertises a single 240-byte entry. A
// MULTIVIEW shader indexing camera[gl_ViewIndex] over six cubemap faces reads
// outside the range its binding declares — undefined, and a Vulkan validation
// error rather than the intended faces.
//
// The publication size is what this pins; the buffer slice the mesh path wraps
// (wrapGpu(m_camerasBuffer, sizeof(CameraUBOData))) has to grow with it.

#include <Gfx/Graph/CameraMath.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

using namespace score::gfx;

namespace
{
std::shared_ptr<ossia::camera_component> makeCamera()
{
  auto c = std::make_shared<ossia::camera_component>();
  c->znear = 0.1f;
  c->zfar = 100.f;
  return c;
}

ossia::scene_spec sceneWithCameras(
    int n, std::vector<ossia::camera_component_ptr>& keepAlive)
{
  std::vector<ossia::scene_payload> children;
  for(int i = 0; i < n; i++)
  {
    auto cam = makeCamera();
    keepAlive.push_back(cam);
    children.push_back(ossia::scene_payload{ossia::camera_component_ptr{cam}});
  }

  auto node = std::make_shared<ossia::scene_node>();
  node->id.value = 1;
  node->children
      = std::make_shared<const std::vector<ossia::scene_payload>>(
          std::move(children));

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::make_shared<const std::vector<ossia::scene_node_ptr>>(
      std::vector<ossia::scene_node_ptr>{node});

  ossia::scene_spec spec;
  spec.state = st;
  return spec;
}
}

TEST_CASE("the camera auxiliary covers every camera the flattener packed", "[scene][flatten][issue163]")
{
  CHECK(sizeof(CameraUBOData) == 240);

  std::vector<ossia::camera_component_ptr> keepAlive;
  FlatScene out;
  flattenScene(sceneWithCameras(6, keepAlive), out, 1.f);
  REQUIRE(out.cameras.size() == 6);

  CHECK(
      cameraAuxByteSize(out.cameras.size())
      == (int64_t)(out.cameras.size() * sizeof(CameraUBOData)));
}

TEST_CASE("the camera auxiliary is correct for the single-camera case", "[scene][flatten][issue163]")
{
  std::vector<ossia::camera_component_ptr> keepAlive;
  FlatScene out;
  flattenScene(sceneWithCameras(1, keepAlive), out, 1.f);
  REQUIRE(out.cameras.size() == 1);
  CHECK(cameraAuxByteSize(out.cameras.size()) == (int64_t)sizeof(CameraUBOData));
}
