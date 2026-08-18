// Tests that are RED on purpose: each one states a contract the engine does
// not currently satisfy. They are kept in their own executable so a defect
// nobody has fixed yet does not hide the green suites next to it, and so the
// day someone fixes one the ctest entry flips to PASS and says so.
//
// Do not "fix" these by weakening the assertion — the assertion IS the report.

#include <Threedim/CameraSwitch.hpp>

#include <Gfx/Graph/CameraMath.hpp>

#include <ossia/detail/variant.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using Catch::Approx;

namespace
{
std::shared_ptr<ossia::scene_state>
make_camera_state(float tx, float yfov, float znear, float zfar)
{
  auto cam = std::make_shared<ossia::camera_component>();
  cam->projection = ossia::camera_projection::perspective;
  cam->yfov = yfov;
  cam->aspect_ratio = 1.f;
  cam->znear = znear;
  cam->zfar = zfar;

  ossia::scene_transform t;
  t.translation[0] = tx;
  t.rotation[3] = 1.f;
  t.scale[0] = t.scale[1] = t.scale[2] = 1.f;

  auto kids = std::make_shared<std::vector<ossia::scene_payload>>();
  kids->push_back(t);
  kids->push_back(ossia::camera_component_ptr(std::move(cam)));

  auto node = std::make_shared<ossia::scene_node>();
  node->id.value = 1 + uint64_t(tx);
  node->children = std::move(kids);

  auto s = std::make_shared<ossia::scene_state>();
  s->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>(
      std::vector<ossia::scene_node_ptr>{node});
  s->version = 1;
  return s;
}

std::string slurp(const std::string& path)
{
  std::ifstream f(path, std::ios::binary);
  REQUIRE(f.good());
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
} // namespace

// ---------------------------------------------------------------------------
// CameraSwitch Blend accumulates onto a DEFAULT-CONSTRUCTED camera_component
// and scene_transform (CameraSwitch.hpp, `ossia::camera_component outCam{}` /
// `ossia::scene_transform outX{}` followed by `+=`). ossia::camera_component
// defaults yfov to 45°, znear to 0.1, zfar to 1000, aspect/xmag/ymag to 1,
// focal_length to 50 — and scene_transform defaults scale to (1,1,1) — so the
// blend result is the weighted mean PLUS those defaults. Blending two
// identical cameras does not reproduce that camera.
//
// Correct behaviour: value-initialise the accumulators (or seed from the
// first weighted input) so the blend is a true weighted mean.
TEST_CASE(
    "DEFECT: CameraSwitch Blend adds camera_component defaults to the mean",
    "[threedim][scene][cameraswitch][known-defect]")
{
  Threedim::CameraSwitch n;
  n.inputs.mode.value = Threedim::CameraSwitch::ins::CameraMode::Blend;
  n.inputs.cam0.scene.state = make_camera_state(0.f, 1.0f, 0.25f, 100.f);
  n.inputs.weights.value = {1.f, 0.f, 0.f, 0.f};
  n.rebuild();
  n();

  const auto& st = n.outputs.scene_out.scene.state;
  REQUIRE(st);
  REQUIRE(st->roots);
  REQUIRE(st->roots->size() == 1);

  const ossia::camera_component* c = nullptr;
  const ossia::scene_transform* t = nullptr;
  for(const auto& child : *(*st->roots)[0]->children)
  {
    if(auto* p = ossia::get_if<ossia::camera_component_ptr>(&child))
      c = p->get();
    else if(auto* x = ossia::get_if<ossia::scene_transform>(&child))
      t = x;
  }
  REQUIRE(c);
  REQUIRE(t);

  // A blend whose whole weight sits on one input must BE that input.
  CHECK(c->yfov == Approx(1.0f));
  CHECK(c->znear == Approx(0.25f));
  CHECK(c->zfar == Approx(100.f));
  CHECK(c->aspect_ratio == Approx(1.f));
  CHECK(t->scale[0] == Approx(1.f));
  CHECK(t->scale[1] == Approx(1.f));
  CHECK(t->scale[2] == Approx(1.f));
}

// ---------------------------------------------------------------------------
// Issue #163. ScenePreprocessorNode keeps a per-camera std140 UBO array and
// uploads m_cachedCameras.size() entries into it, but BOTH sites that publish
// the array as an auxiliary buffer named "camera" advertise
// `byte_size = sizeof(CameraUBOData)` — exactly ONE entry, whatever N is.
// A multiview shader indexing camera[gl_ViewIndex] over six cubemap faces
// therefore reads outside the extent its binding declares.
//
// The contract: for a scene with N cameras, the "camera" auxiliary must
// advertise N * sizeof(CameraUBOData) (clamped to the buffer capacity).
//
// The publication sites are inside a renderer class local to
// ScenePreprocessorNode.cpp, reachable only through a fully-initialised
// RenderList and a downstream consumer's NodeRenderer, so the GPU-free guard
// available today reads the shipped engine source — the ShaderStrings.cpp
// pattern. The entry size itself is pinned for real below.
TEST_CASE(
    "DEFECT #163: the `camera` aux advertises one entry for N cameras",
    "[gfx][scene][camera][163][known-defect]")
{
  const std::string src
      = slurp(std::string(GFX_SRC_DIR) + "/Graph/ScenePreprocessorNode.cpp");

  // Locate every aux publication named "camera" (the mesh/MDI path and the
  // cloud/CSF path) and read the byte_size it advertises.
  std::vector<std::string> sites;
  for(std::size_t p = 0; (p = src.find(".name = \"camera\"", p)) != std::string::npos;
      ++p)
  {
    const std::size_t sz = src.find(".byte_size", p);
    REQUIRE(sz != std::string::npos);
    sites.push_back(src.substr(sz, src.find('}', sz) - sz));
  }
  // Two sites today; a third would need the same treatment.
  REQUIRE(sites.size() >= 2);

  INFO("ScenePreprocessorNode.cpp must publish the FULL packed camera array, "
       "not a single entry — see #163");
  for(const auto& site : sites)
  {
    INFO("site: " << site);
    // The extent must scale with the number of cameras actually uploaded.
    CHECK(site.find("m_cachedCameras.size()") != std::string::npos);
  }
}

TEST_CASE(
    "the camera UBO entry stride is 240 bytes", "[gfx][scene][camera][163]")
{
  // Pinned by a static_assert in CameraMath.hpp; restated here because #163's
  // fix multiplies by exactly this number, and because two tester shaders in
  // the csf-examples corpus encode a superseded 208-byte CameraEntry.
  CHECK(sizeof(score::gfx::CameraUBOData) == 240);
}
