// The shadow_cascades UBO names the light the cascades belong to.
//
// Shadow Cascade Setup records the RawLight slot of the directional light it
// fits the cascades to (the first one casting shadows, else the first one),
// also when its Light direction control overrides the direction. The Scene
// Preprocessor writes that slot into shadow_cascades.light_slot; when the
// setup's input has no such light, the first directional light casting
// shadows in the preprocessor's scene takes it.
//
// Oracle: a probe raster draws the cube in the colour of
// scene_lights.entries[shadow_cascades.light_slot], magenta when the slot is
// 0xFFFFFFFF. Each light has a primary colour, so the readback names the
// light the shaders would shadow.
//
// Chain 1: Cube + Camera + green fill (no shadow) + red key (shadow)
//          -> Shadow Cascade Setup -> Scene Preprocessor <- blue (shadow)
//   red, red with the direction override, then, once the key stops casting,
//   the setup's first directional light: red or green, as the merge of its
//   input orders them, never the blue light only the preprocessor sees.
// Chain 2: Cube + Camera -> Shadow Cascade Setup -> Scene Preprocessor
//          <- green (no shadow), blue (shadow)
//   blue.

#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Threedim/Camera.hpp>
#include <Threedim/Light.hpp>
#include <Threedim/Primitive.hpp>
#include <Threedim/ShadowCascadeSetup.hpp>

#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 64;

constexpr const char* kProbeVS = R"__(void main()
{
    gl_Position = clipSpaceCorrMatrix * camera.viewProjection * vec4(position.xyz, 1.0);
}
)__";

constexpr const char* kProbeFS = R"__(/*{
  "DESCRIPTION": "probe: draws the scene in the colour of the light shadow_cascades.light_slot names, magenta for none.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "TYPES": [
    { "NAME": "Light", "LAYOUT": [
        { "NAME": "color",           "TYPE": "vec4" },
        { "NAME": "local_direction", "TYPE": "vec4" },
        { "NAME": "range_cone",      "TYPE": "vec4" },
        { "NAME": "shadow_enabled",  "TYPE": "uint" },
        { "NAME": "decay_mode",      "TYPE": "uint" },
        { "NAME": "transform_slot",  "TYPE": "uint" },
        { "NAME": "normal_bias",     "TYPE": "float" }
    ] }
  ],
  "INPUTS": [
    { "NAME": "camera", "TYPE": "uniform",
      "LAYOUT": [
        { "NAME": "view",           "TYPE": "mat4" },
        { "NAME": "projection",     "TYPE": "mat4" },
        { "NAME": "viewProjection", "TYPE": "mat4" },
        { "NAME": "cameraPosition", "TYPE": "vec4" },
        { "NAME": "renderSize",     "TYPE": "vec4" },
        { "NAME": "params",         "TYPE": "vec4" }
      ]
    },
    { "NAME": "scene_lights", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "entries", "TYPE": "Light[]" } ] },
    { "NAME": "shadow_cascades", "TYPE": "uniform", "VISIBILITY": "fragment",
      "LAYOUT": [
        { "NAME": "light_view_proj",            "TYPE": "mat4[8]" },
        { "NAME": "cascade_split_distances",    "TYPE": "vec4" },
        { "NAME": "cascade_split_distances_hi", "TYPE": "vec4" },
        { "NAME": "cascade_count",              "TYPE": "uint" },
        { "NAME": "light_slot",                 "TYPE": "uint" },
        { "NAME": "_pad1",                      "TYPE": "uint" },
        { "NAME": "_pad2",                      "TYPE": "uint" }
      ]
    }
  ]
}*/

void main()
{
    uint s = shadow_cascades.light_slot;
    if(shadow_cascades.cascade_count == 0u || s == 0xFFFFFFFFu
       || s >= uint(scene_lights.entries.length()))
        isf_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
    else
        isf_FragColor = vec4(scene_lights.entries[s].color.rgb, 1.0);
}
)__";

bool writeFile(const QString& path, const char* text)
{
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  f.write(text);
  return true;
}

struct HalpProcesses
{
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  int next = 1;

  template <typename T>
  std::unique_ptr<score::gfx::Node> make(const score::DocumentContext& ctx)
  {
    auto model = std::make_unique<oscr::ProcessModel<T>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{next}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));
    return std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<T>{*raw, {}, Gfx::exec_controls{}, next++, ctx}};
  }
};

//! Sets the input at `index` (the field order of the node's ins struct). The
//! node merges messages, so earlier values stay.
void setInput(score::gfx::Node& n, int index, ossia::value v)
{
  score::gfx::Message m;
  m.node_id = n.nodeId;
  m.input.resize(index + 1);
  m.input[index] = std::move(v);
  n.process(std::move(m));
}

//! Light ins: 0 mode, 1 color, 2 intensity, 10 cast shadow, 14 rotation.
void setupLight(score::gfx::Node& n, ossia::vec4f color, bool cast, ossia::vec3f rot)
{
  setInput(n, 1, color);
  setInput(n, 2, 1.f);
  setInput(n, 10, cast);
  setInput(n, 14, rot);
}

//! Modal colour of the drawn pixels, as "r,g,b" bytes rounded to 0 / 255.
std::string drawnColour(const ReadbackImage& img, int& lit)
{
  std::map<std::string, int> hist;
  lit = 0;
  for(int y = 0; y < img.height; ++y)
  {
    for(int x = 0; x < img.width; ++x)
    {
      const auto px = img.at(x, y);
      if(int(px[0]) + int(px[1]) + int(px[2]) < 128)
        continue;
      ++lit;
      std::string k;
      for(int c = 0; c < 3; ++c)
        k += std::string(c ? "," : "") + (px[c] > 127 ? "255" : "0");
      ++hist[k];
    }
  }
  std::string best = "none";
  int n = 0;
  for(const auto& [k, c] : hist)
    if(c > n)
      n = c, best = k;
  return best;
}

constexpr const char* kRed = "255,0,0";
constexpr const char* kGreen = "0,255,0";
constexpr const char* kBlue = "0,0,255";

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  std::vector<std::string> colours;
  std::vector<int> lit;
};

//! `setupHasLights` picks chain 1 or chain 2.
Outcome run_chain(score::gfx::GraphicsApi api, bool setupHasLights)
{
  Outcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = score::test::new_document(app);
    if(!doc)
    {
      out.error = "cannot create a document";
      return;
    }
    QTemporaryDir shaderDir;
    const QString vsPath = shaderDir.filePath("shadow-light-slot.vs");
    const QString fsPath = shaderDir.filePath("shadow-light-slot.fs");
    if(!shaderDir.isValid() || !writeFile(vsPath, kProbeVS)
       || !writeFile(fsPath, kProbeFS))
    {
      out.error = "cannot write the probe shaders";
      return;
    }

    HalpProcesses procs;
    GfxPipeline p;
    const auto& ctx = doc->context();
    const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
    const int cam = p.addNode(procs.make<Threedim::Camera>(ctx));
    const int green = p.addNode(procs.make<Threedim::Light>(ctx));
    const int red = setupHasLights ? p.addNode(procs.make<Threedim::Light>(ctx)) : -2;
    const int blue = p.addNode(procs.make<Threedim::Light>(ctx));
    const int setup = p.addNode(procs.make<Threedim::ShadowCascadeSetup>(ctx));
    const int prep
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(vsPath, fsPath);
    if(cube < 0 || cam < 0 || green < 0 || red == -1 || blue < 0 || setup < 0
       || prep < 0 || raster < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }

    auto* setupIn = p.nodeSceneIn(setup, 0);
    auto* prepIn = p.nodeSceneIn(prep, 0);
    if(!setupIn || !prepIn)
    {
      out.error = "scene ports missing on the chain";
      return;
    }
    p.wire(p.nodeSceneOut(cube, 0), setupIn);
    p.wire(p.nodeSceneOut(cam, 0), setupIn);
    if(setupHasLights)
    {
      p.wire(p.nodeSceneOut(green, 0), setupIn);
      p.wire(p.nodeSceneOut(red, 0), setupIn);
    }
    else
    {
      p.wire(p.nodeSceneOut(green, 0), prepIn);
    }
    p.wire(p.nodeSceneOut(setup, 0), prepIn);
    p.wire(p.nodeSceneOut(blue, 0), prepIn);
    p.wire(p.nodeGeometryOut(prep, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    // Camera ins: eye, target, fov, near, far.
    setInput(*p.node(cam), 0, ossia::vec3f{0.f, 1.f, 3.f});
    setInput(*p.node(cam), 1, ossia::vec3f{0.f, 0.f, 0.f});
    setInput(*p.node(cam), 2, 60.f);
    setupLight(*p.node(green), {0.f, 1.f, 0.f, 1.f}, false, {-40.f, 70.f, 0.f});
    if(setupHasLights)
      setupLight(*p.node(red), {1.f, 0.f, 0.f, 1.f}, true, {-55.f, 30.f, 0.f});
    setupLight(*p.node(blue), {0.f, 0.f, 1.f, 1.f}, true, {-60.f, -20.f, 0.f});

    if(!p.create(api))
    {
      out.backend = p.backend();
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    auto grab = [&] {
      p.render(6);
      int lit = 0;
      out.colours.push_back(drawnColour(p.readback(sink), lit));
      out.lit.push_back(lit);
    };
    grab();
    if(setupHasLights)
    {
      // Shadow Cascade Setup ins: 6 = Light direction.
      setInput(*p.node(setup), 6, ossia::vec3f{-0.5f, -1.f, -0.4f});
      grab();
      setInput(*p.node(red), 10, false);
      grab();
    }
  });
  return out;
}
} // namespace

TEST_CASE(
    "The shadow cascades belong to the light Shadow Cascade Setup fits them to, "
    "with or without its direction override",
    "[gfx][threedim][shadow]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto r = run_chain(api, true);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.colours.size() == 3);
  for(std::size_t i = 0; i < r.colours.size(); ++i)
  {
    INFO("phase " << i << " colour=" << r.colours[i] << " lit=" << r.lit[i]);
    CHECK(r.lit[i] > 40);
  }
  CHECK(r.colours[0] == kRed);
  CHECK(r.colours[1] == kRed);
  CHECK((r.colours[2] == kRed || r.colours[2] == kGreen));
}

TEST_CASE(
    "Without a light in Shadow Cascade Setup's input the cascades go to the "
    "first directional light casting shadows",
    "[gfx][threedim][shadow]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto r = run_chain(api, false);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.colours.size() == 1);
  INFO("colour=" << r.colours[0] << " lit=" << r.lit[0]);
  CHECK(r.lit[0] > 40);
  CHECK(r.colours[0] == kBlue);
}
