// A glTF light casts shadows like a Light with Cast shadow on.
//
// KHR_lights_punctual has no shadow flag, and a light_component casts by
// default, so the Scene Preprocessor writes a loader light's RawLightData with
// shadow_enabled from the light, and gives the cascades of Shadow Cascade
// Setup to the glTF sun it fitted them to although the sun has no RawLight
// slot of its own.
//
// Chain, the one of the gallery's shadow-cascade-setup scene:
//   glTF (ground quad at y = 0, caster quad at y = 1, sun pointing down,
//   point lamp) + Camera -> Shadow Cascade Setup (1 cascade)
//   -> Scene Preprocessor #1 -> depth pass (PER_LAYER, 2 layers: a single
//   layer is a 2D texture, which Scene Resource Route does not route to
//   ShadowMapArray)
//   -> Scene Resource Route (ShadowMapArray)
//   -> Scene Preprocessor #2 (setup output + route) -> receiver.
//
// The receiver draws the ground only and gates the cascade shadow the way
// the scene presets do: the directional light must have shadow_enabled and
// be the light shadow_cascades.light_slot names.
//   R: 1 lit, 0 in the cascade shadow
//   G: 1 when the sun passes that gate
//   B: 1 when the point light has shadow_enabled, which point_shadow_cubemap
//      and the receivers use to pick the point light they shadow.
// The ground under the caster reads (0, 1, 1), the ground away from it
// (1, 1, 1).

#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include "GfxSceneSource.hpp"

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <Threedim/Camera.hpp>
#include <Threedim/GltfParser.hpp>
#include <Threedim/SceneResourceRoute.hpp>
#include <Threedim/ShadowCascadeSetup.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 64;

// Ground [-2, 2]^2 at y = 0 and caster [-0.5, 0.5]^2 at y = 1, normals +Y.
// The sun's rotation turns local -Z to -Y.
constexpr const char* kGltf = R"({
  "asset": { "version": "2.0" },
  "extensionsUsed": [ "KHR_lights_punctual" ],
  "extensions": { "KHR_lights_punctual": { "lights": [
    { "type": "directional", "color": [1, 1, 1], "intensity": 1 },
    { "type": "point", "color": [1, 1, 1], "intensity": 1 }
  ] } },
  "buffers": [ { "byteLength": 156, "uri": "data:application/octet-stream;base64,AAAAwAAAAAAAAADAAAAAQAAAAAAAAADAAAAAQAAAAAAAAABAAAAAwAAAAAAAAABAAAAAvwAAgD8AAAC/AAAAPwAAgD8AAAC/AAAAPwAAgD8AAAA/AAAAvwAAgD8AAAA/AAAAAAAAgD8AAAAAAAAAAAAAgD8AAAAAAAAAAAAAgD8AAAAAAAAAAAAAgD8AAAAAAAACAAEAAAADAAIA" } ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 48 },
    { "buffer": 0, "byteOffset": 48, "byteLength": 48 },
    { "buffer": 0, "byteOffset": 96, "byteLength": 48 },
    { "buffer": 0, "byteOffset": 144, "byteLength": 12 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
      "min": [-2, 0, -2], "max": [2, 0, 2] },
    { "bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3",
      "min": [-0.5, 1, -0.5], "max": [0.5, 1, 0.5] },
    { "bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC3" },
    { "bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR" }
  ],
  "meshes": [
    { "primitives": [ { "attributes": { "POSITION": 0, "NORMAL": 2 }, "indices": 3 } ] },
    { "primitives": [ { "attributes": { "POSITION": 1, "NORMAL": 2 }, "indices": 3 } ] }
  ],
  "nodes": [
    { "name": "ground", "mesh": 0 },
    { "name": "caster", "mesh": 1 },
    { "name": "sun", "rotation": [-0.7071068, 0, 0, 0.7071068],
      "extensions": { "KHR_lights_punctual": { "light": 0 } } },
    { "name": "lamp", "translation": [3, 2, 0],
      "extensions": { "KHR_lights_punctual": { "light": 1 } } }
  ],
  "scenes": [ { "nodes": [0, 1, 2, 3] } ],
  "scene": 0
})";

constexpr const char* kDepthVert = R"(void main()
{
  isf_vertShaderInit();
  vec4 wp = per_draws.data[draw_id].model * vec4(position, 1.0);
  gl_Position = clipSpaceCorrMatrix * shadow_cascades.light_view_proj[PASSINDEX] * wp;
  isf_vertShaderFinish();
}
)";

constexpr const char* kDepthFrag = R"(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "uint", "NAME": "draw_id", "SEMANTIC": "instance_draw_id" }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [],
  "OUTPUTS": [
    { "NAME": "shadow", "TYPE": "depth", "FORMAT": "d32f", "LAYERS": 2,
      "WIDTH": 512, "HEIGHT": 512 }
  ],
  "EXECUTION_MODEL": { "TYPE": "PER_LAYER", "TARGET": "shadow" },
  "TYPES": [
    { "NAME": "PerDraw", "LAYOUT": [
        { "NAME": "model", "TYPE": "mat4" }, { "NAME": "normal", "TYPE": "mat4" },
        { "NAME": "material_index", "TYPE": "uint" }, { "NAME": "tag_hash", "TYPE": "uint" },
        { "NAME": "transform_slot", "TYPE": "uint" }, { "NAME": "skeleton_offset", "TYPE": "uint" } ] },
    { "NAME": "DrawIndexedCmd", "LAYOUT": [
        { "NAME": "indexCount", "TYPE": "uint" }, { "NAME": "instanceCount", "TYPE": "uint" },
        { "NAME": "firstIndex", "TYPE": "uint" }, { "NAME": "baseVertex", "TYPE": "int" },
        { "NAME": "baseInstance", "TYPE": "uint" } ] }
  ],
  "INPUTS": [
    { "NAME": "shadow_cascades", "TYPE": "uniform", "VISIBILITY": "vertex",
      "LAYOUT": [
        { "NAME": "light_view_proj", "TYPE": "mat4[8]" },
        { "NAME": "cascade_split_distances", "TYPE": "vec4" },
        { "NAME": "cascade_split_distances_hi", "TYPE": "vec4" },
        { "NAME": "cascade_count", "TYPE": "uint" }, { "NAME": "light_slot", "TYPE": "uint" },
        { "NAME": "_pad1", "TYPE": "uint" }, { "NAME": "_pad2", "TYPE": "uint" } ] },
    { "NAME": "per_draws", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "vertex",
      "LAYOUT": [ { "NAME": "data", "TYPE": "PerDraw[]" } ] },
    { "NAME": "indirect_draw_cmds", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "none",
      "BUFFER_USAGE": "indirect_draw_indexed",
      "LAYOUT": [ { "NAME": "cmds", "TYPE": "DrawIndexedCmd[]" } ] }
  ],
  "PIPELINE_STATE": { "DEPTH_TEST": true, "DEPTH_WRITE": true, "COLOR_WRITE": false,
                      "CULL_MODE": "none" }
}*/
void main() { }
)";

constexpr const char* kRecvVert = R"(void main()
{
  isf_vertShaderInit();
  vec4 wp = per_draws.data[draw_id].model * vec4(position, 1.0);
  v_world = wp.xyz;
  gl_Position = clipSpaceCorrMatrix * camera.viewProjection * wp;
  isf_vertShaderFinish();
}
)";

constexpr const char* kRecvFrag = R"(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "uint", "NAME": "draw_id", "SEMANTIC": "instance_draw_id" }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec3", "NAME": "v_world" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec3", "NAME": "v_world" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": { "CULL_MODE": "none" },
  "TYPES": [
    { "NAME": "Light", "LAYOUT": [
        { "NAME": "color", "TYPE": "vec4" }, { "NAME": "local_direction", "TYPE": "vec4" },
        { "NAME": "range_cone", "TYPE": "vec4" }, { "NAME": "shadow_enabled", "TYPE": "uint" },
        { "NAME": "decay_mode", "TYPE": "uint" }, { "NAME": "transform_slot", "TYPE": "uint" },
        { "NAME": "normal_bias", "TYPE": "float" } ] },
    { "NAME": "PerDraw", "LAYOUT": [
        { "NAME": "model", "TYPE": "mat4" }, { "NAME": "normal", "TYPE": "mat4" },
        { "NAME": "material_index", "TYPE": "uint" }, { "NAME": "tag_hash", "TYPE": "uint" },
        { "NAME": "transform_slot", "TYPE": "uint" }, { "NAME": "skeleton_offset", "TYPE": "uint" } ] },
    { "NAME": "DrawIndexedCmd", "LAYOUT": [
        { "NAME": "indexCount", "TYPE": "uint" }, { "NAME": "instanceCount", "TYPE": "uint" },
        { "NAME": "firstIndex", "TYPE": "uint" }, { "NAME": "baseVertex", "TYPE": "int" },
        { "NAME": "baseInstance", "TYPE": "uint" } ] }
  ],
  "INPUTS": [
    { "NAME": "shadow_map_array", "TYPE": "image", "IS_ARRAY": true,
      "FILTER": "nearest", "WRAP": "clamp" },
    { "NAME": "camera", "TYPE": "uniform", "VISIBILITY": "vertex",
      "LAYOUT": [
        { "NAME": "view", "TYPE": "mat4" }, { "NAME": "projection", "TYPE": "mat4" },
        { "NAME": "viewProjection", "TYPE": "mat4" }, { "NAME": "cameraPosition", "TYPE": "vec4" },
        { "NAME": "renderSize", "TYPE": "vec4" }, { "NAME": "params", "TYPE": "vec4" } ] },
    { "NAME": "shadow_cascades", "TYPE": "uniform", "VISIBILITY": "fragment",
      "LAYOUT": [
        { "NAME": "light_view_proj", "TYPE": "mat4[8]" },
        { "NAME": "cascade_split_distances", "TYPE": "vec4" },
        { "NAME": "cascade_split_distances_hi", "TYPE": "vec4" },
        { "NAME": "cascade_count", "TYPE": "uint" }, { "NAME": "light_slot", "TYPE": "uint" },
        { "NAME": "_pad1", "TYPE": "uint" }, { "NAME": "_pad2", "TYPE": "uint" } ] },
    { "NAME": "scene_lights", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "entries", "TYPE": "Light[]" } ] },
    { "NAME": "scene_light_indices", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "data", "TYPE": "uint[]" } ] },
    { "NAME": "scene_counts", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "light_count", "TYPE": "uint" }, { "NAME": "material_count", "TYPE": "uint" },
                  { "NAME": "draw_count", "TYPE": "uint" }, { "NAME": "_pad0", "TYPE": "uint" } ] },
    { "NAME": "per_draws", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "vertex",
      "LAYOUT": [ { "NAME": "data", "TYPE": "PerDraw[]" } ] },
    { "NAME": "indirect_draw_cmds", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "none",
      "BUFFER_USAGE": "indirect_draw_indexed",
      "LAYOUT": [ { "NAME": "cmds", "TYPE": "DrawIndexedCmd[]" } ] }
  ]
}*/
void main()
{
  if(v_world.y > 0.5)
    discard;
  bool csm = false;
  bool pointShadow = false;
  for(uint i = 0u; i < scene_counts.light_count; i++)
  {
    uint s = scene_light_indices.data[i];
    Light L = scene_lights.entries[s];
    if(L.shadow_enabled == 0u)
      continue;
    if(L.local_direction.w < 0.5)
      csm = csm || (s == shadow_cascades.light_slot);
    else if(L.local_direction.w < 1.5)
      pointShadow = true;
  }
  csm = csm && shadow_cascades.cascade_count > 0u
        && textureSize(shadow_map_array, 0).x > 1;

  float lit = 1.0;
  if(csm)
  {
    vec4 lightClip = shadow_cascades.light_view_proj[0] * vec4(v_world, 1.0);
    vec4 lightTex = clipSpaceCorrMatrix * lightClip;
    vec2 uv = lightTex.xy / lightTex.w * 0.5 + 0.5;
    float refDepth = lightClip.z / lightClip.w * 0.5 + 0.5;
    float z = texture(shadow_map_array, vec3(uv, 0.0)).r;
    // Reverse-Z: an occluder nearer the light stores a larger depth.
    if(refDepth < z - 0.01)
      lit = 0.0;
  }
  isf_FragColor = vec4(lit, csm ? 1.0 : 0.0, pointShadow ? 1.0 : 0.0, 1.0);
}
)";

QString writeText(const QTemporaryDir& dir, const char* name, const char* text)
{
  const QString path = dir.filePath(QString::fromUtf8(name));
  QFile f(path);
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  f.write(text);
  return path;
}

std::shared_ptr<ossia::scene_state> loadGltf(const QString& path)
{
  using gltf_port = Threedim::GltfParser::ins::gltf_t;
  gltf_port::file_type file{};
  const std::string p = path.toStdString();
  file.filename = p;
  auto apply = gltf_port::process(file);
  if(!apply)
    return {};
  Threedim::GltfParser parser;
  apply(parser);
  if(!parser.m_raw_state)
    return {};
  return std::make_shared<ossia::scene_state>(*parser.m_raw_state);
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

//! Sets the input at `index` (the field order of the node's ins struct).
void setInput(score::gfx::Node& n, int index, ossia::value v)
{
  score::gfx::Message m;
  m.node_id = n.nodeId;
  m.input.resize(index + 1);
  m.input[index] = std::move(v);
  n.process(std::move(m));
}
}

TEST_CASE(
    "A glTF light casts the cascade shadows of Shadow Cascade Setup",
    "[gfx][scene][gltf][light][shadow]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString gltf = writeText(dir, "shadows.gltf", kGltf);
  const QString dvs = writeText(dir, "depth.vert", kDepthVert);
  const QString dfs = writeText(dir, "depth.frag", kDepthFrag);
  const QString rvs = writeText(dir, "recv.vert", kRecvVert);
  const QString rfs = writeText(dir, "recv.frag", kRecvFrag);
  auto state = loadGltf(gltf);
  REQUIRE(state);
  REQUIRE(state->roots);

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = score::test::new_document(app);
    if(!doc)
    {
      err = "cannot create a document";
      return;
    }
    const auto& ctx = doc->context();
    HalpProcesses procs;
    GfxPipeline p;
    const int src = p.addNode(std::make_unique<scene::StaticSceneNode>(state));
    const int cam = p.addNode(procs.make<Threedim::Camera>(ctx));
    const int setup = p.addNode(procs.make<Threedim::ShadowCascadeSetup>(ctx));
    const int prep1 = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int depth = p.addRaster(dvs, dfs);
    const int route = p.addNode(procs.make<Threedim::SceneResourceRoute>(ctx));
    const int prep2 = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int recv = p.addRaster(rvs, rfs);
    if(src < 0 || cam < 0 || setup < 0 || prep1 < 0 || depth < 0 || route < 0
       || prep2 < 0 || recv < 0)
    {
      err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(src, 0), p.nodeSceneIn(setup, 0));
    p.wire(p.nodeSceneOut(cam, 0), p.nodeSceneIn(setup, 0));
    p.wire(p.nodeSceneOut(setup, 0), p.nodeSceneIn(prep1, 0));
    p.wire(p.nodeGeometryOut(prep1, 0), p.geometryIn(depth, 0));
    p.wire(p.imageOut(depth, 0), nth_image_input(*p.node(route), 0));
    p.wire(p.nodeSceneOut(setup, 0), p.nodeSceneIn(prep2, 0));
    p.wire(p.nodeSceneOut(route, 0), p.nodeSceneIn(prep2, 0));
    p.wire(p.nodeGeometryOut(prep2, 0), p.geometryIn(recv, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(recv, 0), p.sinkInput(sink));

    // Camera ins: eye, target, fov, near, far.
    setInput(*p.node(cam), 0, ossia::vec3f{0.f, 4.f, 0.5f});
    setInput(*p.node(cam), 1, ossia::vec3f{0.f, 0.f, 0.f});
    setInput(*p.node(cam), 2, 60.f);
    // Shadow Cascade Setup ins: 1 cascade count, 2 shadow distance,
    // 4 camera near, 5 camera far.
    setInput(*p.node(setup), 1, 1);
    setInput(*p.node(setup), 2, 8.f);
    setInput(*p.node(setup), 4, 0.1f);
    setInput(*p.node(setup), 5, 8.f);
    // Scene Resource Route ins: 1 target field.
    setInput(*p.node(route), 1, int(Threedim::SceneResourceTarget::ShadowMapArray));

    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(8);
    img = p.readback(sink);
  });
  if(skipped)
    SKIP("backend unavailable");
  REQUIRE(err.empty());
  REQUIRE(img.valid());

  // The caster's shadow lands on the ground around the origin, which the
  // camera sees at the centre; x = -1.6 on the ground is lit.
  const auto under = img.center();
  const auto away = img.at(kSize / 8, kSize / 2);
  INFO("under " << scene::rgba_string(under) << " away " << scene::rgba_string(away));
  CHECK(int(under[1]) > 200);
  CHECK(int(under[0]) < 50);
  CHECK(int(away[0]) > 200);
  CHECK(int(away[1]) > 200);
  CHECK(int(under[2]) > 200);
}
