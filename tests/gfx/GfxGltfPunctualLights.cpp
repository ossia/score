// KHR_lights_punctual lights inside a glTF light the scene.
//
// A glTF holding a quad facing +Z, a red directional light tilted 60 degrees
// away from the quad's normal and a green point light 2 units in front of it
// (two nested node translations of 1) goes through GltfParser, the
// ScenePreprocessor and a raster shader that reads the lights the way the
// scene presets do: scene_light_indices -> scene_lights ->
// world_transforms[transform_slot]. The quad's centre is lit by both: red
// N.L = cos(60) = 0.5, green 1 / 2^2 = 0.25.

#include <score_test/Gfx.hpp>

#include "GfxSceneSource.hpp"

#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <Threedim/GltfParser.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>
#include <QTemporaryDir>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 64;

// Quad in [-0.8, 0.8]^2 at z = 0, normals +Z, indexed.
constexpr const char* kGltf = R"({
  "asset": { "version": "2.0" },
  "extensionsUsed": [ "KHR_lights_punctual" ],
  "extensions": { "KHR_lights_punctual": { "lights": [
    { "type": "directional", "color": [1, 0, 0], "intensity": 1 },
    { "type": "point", "color": [0, 1, 0], "intensity": 1 }
  ] } },
  "buffers": [ { "byteLength": 108, "uri": "data:application/octet-stream;base64,zcxMv83MTL8AAAAAzcxMP83MTL8AAAAAzcxMP83MTD8AAAAAzcxMv83MTD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAABAAIAAAACAAMA" } ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 48 },
    { "buffer": 0, "byteOffset": 48, "byteLength": 48 },
    { "buffer": 0, "byteOffset": 96, "byteLength": 12 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
      "min": [-0.8, -0.8, 0], "max": [0.8, 0.8, 0] },
    { "bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3" },
    { "bufferView": 2, "componentType": 5123, "count": 6, "type": "SCALAR" }
  ],
  "meshes": [ { "primitives": [ { "attributes": { "POSITION": 0, "NORMAL": 1 }, "indices": 2 } ] } ],
  "nodes": [
    { "name": "quad", "mesh": 0 },
    { "name": "sun", "rotation": [0.5, 0, 0, 0.8660254],
      "extensions": { "KHR_lights_punctual": { "light": 0 } } },
    { "name": "lamp_parent", "translation": [0, 0, 1], "children": [3] },
    { "name": "lamp", "translation": [0, 0, 1],
      "extensions": { "KHR_lights_punctual": { "light": 1 } } }
  ],
  "scenes": [ { "nodes": [0, 1, 2] } ],
  "scene": 0
})";

constexpr const char* kVert = R"(void main()
{
  isf_vertShaderInit();
  v_pos = position;
  v_nrm = normal;
  gl_Position = clipSpaceCorrMatrix * vec4(position.xy, 0.5, 1.0);
  isf_vertShaderFinish();
}
)";

constexpr const char* kFrag = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "vec3", "NAME": "normal" }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec3", "NAME": "v_pos" }, { "TYPE": "vec3", "NAME": "v_nrm" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec3", "NAME": "v_pos" }, { "TYPE": "vec3", "NAME": "v_nrm" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": { "CULL_MODE": "none" },
  "TYPES": [
    { "NAME": "Light", "LAYOUT": [
        { "NAME": "color", "TYPE": "vec4" }, { "NAME": "local_direction", "TYPE": "vec4" },
        { "NAME": "range_cone", "TYPE": "vec4" }, { "NAME": "shadow_enabled", "TYPE": "uint" },
        { "NAME": "decay_mode", "TYPE": "uint" }, { "NAME": "transform_slot", "TYPE": "uint" },
        { "NAME": "normal_bias", "TYPE": "float" } ] }
  ],
  "INPUTS": [
    { "NAME": "scene_lights", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "entries", "TYPE": "Light[]" } ] },
    { "NAME": "scene_light_indices", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "data", "TYPE": "uint[]" } ] },
    { "NAME": "world_transforms", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "data", "TYPE": "mat4[]" } ] },
    { "NAME": "scene_counts", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "light_count", "TYPE": "uint" }, { "NAME": "material_count", "TYPE": "uint" },
                  { "NAME": "draw_count", "TYPE": "uint" }, { "NAME": "_pad0", "TYPE": "uint" } ] }
  ]
}*/
void main()
{
  vec3 N = normalize(v_nrm);
  vec3 sum = vec3(0.0);
  for(uint i = 0u; i < scene_counts.light_count; i++)
  {
    Light L = scene_lights.entries[scene_light_indices.data[i]];
    mat4 W = world_transforms.data[L.transform_slot];
    vec3 radiance = L.color.rgb * L.color.w;
    if(L.local_direction.w == 0.0)
    {
      vec3 dir = normalize((W * vec4(L.local_direction.xyz, 0.0)).xyz);
      sum += radiance * max(dot(N, -dir), 0.0);
    }
    else
    {
      vec3 toLight = W[3].xyz - v_pos;
      float d2 = max(dot(toLight, toLight), 1e-4);
      sum += radiance / d2 * max(dot(N, normalize(toLight)), 0.0);
    }
  }
  isf_FragColor = vec4(sum, 1.0);
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
}

TEST_CASE("glTF punctual lights light the scene", "[gfx][scene][gltf][light]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString gltf = writeText(dir, "lights.gltf", kGltf);
  const QString vs = writeText(dir, "lights.vert", kVert);
  const QString fs = writeText(dir, "lights.frag", kFrag);
  auto state = loadGltf(gltf);
  REQUIRE(state);
  REQUIRE(state->roots);

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int hn = p.addNode(std::make_unique<scene::StaticSceneNode>(state));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(vs, fs);
    if(hn < 0 || flat < 0 || raster < 0)
    {
      err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(hn, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(5);
    img = p.readback(sink);
  });
  if(skipped)
    SKIP("backend unavailable");
  REQUIRE(err.empty());
  REQUIRE(img.valid());
  const auto c = img.center();
  INFO("center " << scene::rgba_string(c));
  // Directional: 0.5 x 255.
  CHECK(int(c[0]) > 118);
  CHECK(int(c[0]) < 138);
  // Point: 0.25 x 255.
  CHECK(int(c[1]) > 54);
  CHECK(int(c[1]) < 74);
  CHECK(int(c[2]) < 4);
}
