// ScenePreprocessorNode regressions, each driven through the real node into a
// RAW_RASTER_PIPELINE or CSF consumer and read back as pixels:
//  - Material arena slot 0 (the default material of a mesh without one) is
//    white even when the RenderList's initial batch, which carries the
//    registry seed, is dropped by a resize before the first frame.
//  - Two preprocessors feeding one output each bind their own merged
//    environment (consumers bind an aux UBO from offset 0).
//  - A scene_node with visible == false draws nothing.
//  - A primitive-cloud bucket's buffers are sized to the primitive count, so
//    a CSF's .length() over a bucket attribute is the primitive count.
//  - Moving a primitive cloud re-uploads its cloud_meta only: its rows (a
//    whole splat file) stay where they are, and the consumer sees the new
//    model matrix.
//  - Two preprocessors share the registry's texture pool: when the second one
//    adds a texture, the first one's layer is kept, and when that grows a
//    bucket (a new QRhiTexture array) the first one republishes, so its
//    classic_pbr_full consumer keeps sampling its own texture from a live
//    array instead of the deleted one.
//  - A scene without an environment has no ambient light.
//  - Alpha-blended draws are drawn after the opaque ones, back to front, so
//    they composite over what is behind them.

#include <score_test/Gfx.hpp>

#include "GfxSceneSource.hpp"
#include "GfxUserLibrary.hpp"

#include <Gfx/Graph/FlattenedSceneFilterNode.hpp>
#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QImage>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <array>
#include <cstring>

using namespace score::test::gfx;
using scene::StaticSceneNode;

namespace
{
constexpr int kSize = 64;

QString writeText(const QTemporaryDir& dir, const char* name, const char* text)
{
  const QString path = dir.filePath(QString::fromUtf8(name));
  QFile f(path);
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  f.write(text);
  return path;
}

constexpr const char* kVert = R"(void main()
{
  isf_vertShaderInit();
  v_drawID = draw_id;
  gl_Position = clipSpaceCorrMatrix * vec4(position.xy, 0.5, 1.0);
  isf_vertShaderFinish();
}
)";

#define SCENE_RASTER_HEADER                                                         \
  R"("ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "uint", "NAME": "draw_id", "SEMANTIC": "instance_draw_id", "REQUIRED": true }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "uint", "NAME": "v_drawID" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "uint", "NAME": "v_drawID" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": { "CULL_MODE": "none" },)"

constexpr const char* kMaterialFrag = "/*{" SCENE_RASTER_HEADER R"(
  "INPUTS": [
    { "NAME": "per_draws", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "vertex+fragment",
      "LAYOUT": [ { "NAME": "data", "TYPE": "PerDraw[]" } ] },
    { "NAME": "scene_materials", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "entries", "TYPE": "Material[]" } ] }
  ],
  "TYPES": [
    { "NAME": "PerDraw", "LAYOUT": [
        { "NAME": "model", "TYPE": "mat4" }, { "NAME": "normal", "TYPE": "mat4" },
        { "NAME": "material_index", "TYPE": "uint" }, { "NAME": "tag_hash", "TYPE": "uint" },
        { "NAME": "transform_slot", "TYPE": "uint" }, { "NAME": "skeleton_offset", "TYPE": "uint" } ] },
    { "NAME": "Material", "LAYOUT": [
        { "NAME": "baseColor", "TYPE": "vec4" }, { "NAME": "metallicRoughnessOcclusionUnlit", "TYPE": "vec4" },
        { "NAME": "emissive_strength", "TYPE": "vec4" }, { "NAME": "textureRefs", "TYPE": "uvec4" },
        { "NAME": "feature_mask", "TYPE": "uint" }, { "NAME": "hit_group_id", "TYPE": "uint" },
        { "NAME": "occlusion_textureRef", "TYPE": "uint" }, { "NAME": "alpha_cutoff", "TYPE": "float" } ] }
  ]
}*/
void main()
{
  uint mi = per_draws.data[v_drawID].material_index;
  isf_FragColor = vec4(scene_materials.entries[mi].baseColor.rgb, 1.0);
}
)";

constexpr const char* kEnvFrag = "/*{" SCENE_RASTER_HEADER R"(
  "INPUTS": [
    { "NAME": "env", "TYPE": "uniform", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "ambient", "TYPE": "vec4" }, { "NAME": "fog_color_density", "TYPE": "vec4" },
                  { "NAME": "fog_range", "TYPE": "vec4" }, { "NAME": "exposure_gamma", "TYPE": "vec4" } ] }
  ]
}*/
void main()
{
  isf_FragColor = vec4(env.ambient.rgb * env.ambient.w, 1.0);
}
)";

// Scales the ambient term so that the 0.03 grey once used as the default
// saturates the readback.
constexpr const char* kEnvAmbientScaledFrag = "/*{" SCENE_RASTER_HEADER R"(
  "INPUTS": [
    { "NAME": "env", "TYPE": "uniform", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "ambient", "TYPE": "vec4" }, { "NAME": "fog_color_density", "TYPE": "vec4" },
                  { "NAME": "fog_range", "TYPE": "vec4" }, { "NAME": "exposure_gamma", "TYPE": "vec4" } ] }
  ]
}*/
void main()
{
  isf_FragColor = vec4(env.ambient.rgb * env.ambient.w * 100.0, 1.0);
}
)";

constexpr const char* kDepthVert = R"(void main()
{
  isf_vertShaderInit();
  v_drawID = draw_id;
  gl_Position = clipSpaceCorrMatrix * vec4(position.xy, position.z, 1.0);
  isf_vertShaderFinish();
}
)";

// Draws each material's base colour with its alpha, blended over, with depth
// test and write on as the scene presets do.
constexpr const char* kBlendFrag = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" },
    { "TYPE": "uint", "NAME": "draw_id", "SEMANTIC": "instance_draw_id", "REQUIRED": true }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "uint", "NAME": "v_drawID" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "uint", "NAME": "v_drawID" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": true, "DEPTH_WRITE": true, "CULL_MODE": "none",
    "BLEND": {
      "ENABLE": true,
      "SRC_COLOR": "src_alpha", "DST_COLOR": "one_minus_src_alpha", "OP_COLOR": "add",
      "SRC_ALPHA": "one", "DST_ALPHA": "one_minus_src_alpha", "OP_ALPHA": "add"
    }
  },
  "INPUTS": [
    { "NAME": "per_draws", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "vertex+fragment",
      "LAYOUT": [ { "NAME": "data", "TYPE": "PerDraw[]" } ] },
    { "NAME": "scene_materials", "TYPE": "storage", "ACCESS": "read_only", "VISIBILITY": "fragment",
      "LAYOUT": [ { "NAME": "entries", "TYPE": "Material[]" } ] }
  ],
  "TYPES": [
    { "NAME": "PerDraw", "LAYOUT": [
        { "NAME": "model", "TYPE": "mat4" }, { "NAME": "normal", "TYPE": "mat4" },
        { "NAME": "material_index", "TYPE": "uint" }, { "NAME": "tag_hash", "TYPE": "uint" },
        { "NAME": "transform_slot", "TYPE": "uint" }, { "NAME": "skeleton_offset", "TYPE": "uint" } ] },
    { "NAME": "Material", "LAYOUT": [
        { "NAME": "baseColor", "TYPE": "vec4" }, { "NAME": "metallicRoughnessOcclusionUnlit", "TYPE": "vec4" },
        { "NAME": "emissive_strength", "TYPE": "vec4" }, { "NAME": "textureRefs", "TYPE": "uvec4" },
        { "NAME": "feature_mask", "TYPE": "uint" }, { "NAME": "hit_group_id", "TYPE": "uint" },
        { "NAME": "occlusion_textureRef", "TYPE": "uint" }, { "NAME": "alpha_cutoff", "TYPE": "float" } ] }
  ]
}*/
void main()
{
  uint mi = per_draws.data[v_drawID].material_index;
  isf_FragColor = scene_materials.entries[mi].baseColor;
}
)";

constexpr const char* kSolidFrag = "/*{" SCENE_RASTER_HEADER R"(
  "INPUTS": []
}*/
void main()
{
  isf_FragColor = vec4(1.0);
}
)";

constexpr const char* kSideBySide = R"(/*{
  "ISFVSN": "2.0",
  "INPUTS": [ { "NAME": "imgA", "TYPE": "image" }, { "NAME": "imgB", "TYPE": "image" } ]
}*/
void main()
{
  vec2 uv = isf_FragNormCoord;
  gl_FragColor = uv.x < 0.5 ? IMG_NORM_PIXEL(imgA, uv) : IMG_NORM_PIXEL(imgB, uv);
}
)";

constexpr const char* kLengthCsf = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "64", "HEIGHT": "64" },
    {
      "NAME": "geoOut", "TYPE": "geometry", "VERTEX_COUNT": "$VERTEX_COUNT_geoIn",
      "ATTRIBUTES": [ { "NAME": "dummy", "SEMANTIC": "dummy", "TYPE": "float", "ACCESS": "write_only" } ],
      "AUXILIARY": [ { "NAME": "sized", "ACCESS": "read_write", "SIZE": "$VERTEX_COUNT_geoIn", "LAYOUT": [ { "NAME": "values", "TYPE": "uint[]" } ] } ]
    },
    {
      "NAME": "geoIn", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "cloud_id", "SEMANTIC": "custom", "TYPE": "uint", "ACCESS": "read_only" } ]
    }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  if (p.x >= imageSize(outputImage).x || p.y >= imageSize(outputImage).y) return;
  float n_len  = float(ISF_READ(geoIn, cloud_id).length());
  IMG_STORE(outputImage, p, vec4(n_len / 255.0, float(sized.values.length() > 0), 0.0, 1.0));
}
)";

constexpr const char* kCloudMetaCsf = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "TYPES": [
    { "NAME": "CloudMeta", "LAYOUT": [
        { "NAME": "model", "TYPE": "mat4" }, { "NAME": "bounds_min", "TYPE": "vec4" },
        { "NAME": "bounds_max", "TYPE": "vec4" }, { "NAME": "primitive_offset", "TYPE": "uint" },
        { "NAME": "primitive_count", "TYPE": "uint" }, { "NAME": "transform_slot", "TYPE": "uint" },
        { "NAME": "format_param_index", "TYPE": "uint" }, { "NAME": "_pad0", "TYPE": "uvec4" } ] }
  ],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "64", "HEIGHT": "64" },
    {
      "NAME": "geoIn", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "cloud_id", "SEMANTIC": "custom", "TYPE": "uint", "ACCESS": "read_only" } ],
      "AUXILIARY": [
        { "NAME": "cloud_meta", "ACCESS": "read_only", "LAYOUT": [ { "NAME": "entries", "TYPE": "CloudMeta[]" } ] },
        { "NAME": "raw_splats", "ACCESS": "read_only", "LAYOUT": [ { "NAME": "values", "TYPE": "float[]" } ] }
      ]
    }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  if (p.x >= imageSize(outputImage).x || p.y >= imageSize(outputImage).y) return;
  uint cid = ISF_READ(geoIn, cloud_id)[0];
  float tx = cloud_meta.entries[cid].model[3].x;
  IMG_STORE(outputImage, p, vec4(tx, raw_splats.values[4] / 255.0, 0.0, 1.0));
}
)";

std::shared_ptr<ossia::buffer_resource>
cpuBuffer(std::vector<float> data, ossia::buffer_data::usage usage)
{
  auto owned = std::make_shared<std::vector<float>>(std::move(data));
  auto res = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(owned, owned->data());
  bd.byte_size = int64_t(owned->size() * sizeof(float));
  bd.usage_hint = usage;
  res->resource = bd;
  res->dirty_index = 1;
  res->content_hash = (uint64_t)(uintptr_t)owned->data();
  return res;
}

// Axis-aligned quad in NDC, x in [x0, x1], full height, no material.
ossia::mesh_component_ptr quad(float x0, float x1, uint64_t id)
{
  auto pos = cpuBuffer(
      {x0, -1, 0, x1, -1, 0, x1, 1, 0, x0, -1, 0, x1, 1, 0, x0, 1, 0},
      ossia::buffer_data::usage::vertex_buffer);
  ossia::mesh_primitive prim;
  prim.vertex_buffers = {pos};
  ossia::vertex_attribute p;
  p.semantic = ossia::attribute_semantic::position;
  p.format = ossia::vertex_format::float3;
  p.buffer_index = 0;
  p.byte_stride = 12;
  prim.attributes.push_back(p);
  prim.topology = ossia::primitive_topology::triangles;
  prim.vertex_count = 6;
  prim.stable_id = id;
  prim.bounds = {{x0, -1.f, 0.f}, {x1, 1.f, 0.f}};

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->dirty_index = 1;
  return mesh;
}

ossia::scene_node_ptr nodeWith(ossia::scene_payload payload, uint64_t id, bool visible = true)
{
  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(std::move(payload));
  auto n = std::make_shared<ossia::scene_node>();
  n->id.value = id;
  n->visible = visible;
  n->children = std::move(children);
  return n;
}

std::shared_ptr<ossia::scene_state> stateWith(std::vector<ossia::scene_node_ptr> roots)
{
  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>(std::move(roots));
  st->version = 1;
  st->dirty_index = 1;
  return st;
}

struct Result
{
  bool skipped = false;
  std::string err;
  ReadbackImage img;
};

// scene -> ScenePreprocessor -> raster; returns the raster node index.
int sceneChain(
    GfxPipeline& p, std::shared_ptr<ossia::scene_state> st, const QString& vs,
    const QString& fs)
{
  const int hn = p.addNode(std::make_unique<StaticSceneNode>(std::move(st)));
  const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
  const int raster = p.addRaster(vs, fs);
  if(hn < 0 || flat < 0 || raster < 0)
    return -1;
  p.wire(p.nodeSceneOut(hn, 0), p.nodeSceneIn(flat, 0));
  p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
  return raster;
}

ossia::material_component_ptr solidTextured(QColor color, uint64_t id)
{
  QImage img(64, 64, QImage::Format_RGBA8888);
  img.fill(color);
  auto m = std::make_shared<ossia::material_component>();
  m->stable_id = id;
  m->unlit = true;
  m->base_color_texture.source = scene::png_source(img);
  return m;
}

std::shared_ptr<ossia::scene_state>
texturedQuad(ossia::material_component_ptr mat, uint64_t id, int64_t version)
{
  constexpr float e = 0.8f;
  auto pos = cpuBuffer(
      {-e, -e, 0, e, -e, 0, e, e, 0, -e, -e, 0, e, e, 0, -e, e, 0},
      ossia::buffer_data::usage::vertex_buffer);
  auto uv = cpuBuffer(
      {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1}, ossia::buffer_data::usage::vertex_buffer);

  ossia::mesh_primitive prim;
  prim.vertex_buffers = {pos, uv};
  ossia::vertex_attribute p;
  p.semantic = ossia::attribute_semantic::position;
  p.format = ossia::vertex_format::float3;
  p.buffer_index = 0;
  p.byte_stride = 12;
  prim.attributes.push_back(p);
  ossia::vertex_attribute t;
  t.semantic = ossia::attribute_semantic::texcoord0;
  t.format = ossia::vertex_format::float2;
  t.buffer_index = 1;
  t.byte_stride = 8;
  prim.attributes.push_back(t);
  prim.topology = ossia::primitive_topology::triangles;
  prim.vertex_count = 6;
  prim.stable_id = id;
  prim.bounds = {{-e, -e, 0.f}, {e, e, 0.f}};
  prim.material = mat;

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->dirty_index = 1;

  auto st = stateWith({nodeWith(ossia::mesh_component_ptr(std::move(mesh)), id)});
  st->materials = std::make_shared<std::vector<ossia::material_component_ptr>>(
      std::vector<ossia::material_component_ptr>{mat});
  st->version = version;
  return st;
}

}

TEST_CASE(
    "a mesh without a material reads the white default material after a "
    "RenderList rebuild before the first frame",
    "[gfx][scene][material]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "scenepp.vert", kVert);
  const QString fs = writeText(dir, "scenepp_material.frag", kMaterialFrag);

  Result r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = sceneChain(p, stateWith({nodeWith(quad(-1, 1, 0xF1A0001u), 11)}), vs, fs);
    if(raster < 0)
    {
      r.err = "chain build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.err = r.skipped ? std::string{} : p.error();
      return;
    }
    // The resize tears the first RenderList down before it has rendered.
    p.resizeSink(sink, {kSize, kSize + 8});
    p.render(5);
    r.img = p.readback(sink);
  });
  if(r.skipped)
    SKIP("backend unavailable");
  REQUIRE(r.err.empty());
  REQUIRE(r.img.valid());
  const auto c = r.img.center();
  INFO("center " << scene::rgba_string(c));
  CHECK(c[0] > 240);
  CHECK(c[1] > 240);
  CHECK(c[2] > 240);
}

TEST_CASE(
    "two scene preprocessors on one output each bind their own environment",
    "[gfx][scene][environment]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "scenepp.vert", kVert);
  const QString fs = writeText(dir, "scenepp_env.frag", kEnvFrag);
  const QString combine = writeText(dir, "scenepp_side.fs", kSideBySide);

  const auto withAmbient = [](float r, float g, float b, uint64_t id) {
    auto st = stateWith({nodeWith(quad(-1, 1, id), id)});
    st->environment.ambient_color[0] = r;
    st->environment.ambient_color[1] = g;
    st->environment.ambient_color[2] = b;
    st->environment.ambient_intensity = 1.f;
    st->environment.params_set |= ossia::scene_environment::params_ambient;
    return st;
  };

  Result r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = sceneChain(p, withAmbient(1, 0, 0, 0xF1A0011u), vs, fs);
    const int b = sceneChain(p, withAmbient(0, 0, 1, 0xF1A0012u), vs, fs);
    const int side = p.addIsf(combine);
    if(a < 0 || b < 0 || side < 0)
    {
      r.err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.imageOut(a, 0), p.imageIn(side, 0));
    p.wire(p.imageOut(b, 0), p.imageIn(side, 1));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(side, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.err = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(5);
    r.img = p.readback(sink);
  });
  if(r.skipped)
    SKIP("backend unavailable");
  REQUIRE(r.err.empty());
  REQUIRE(r.img.valid());
  const auto left = r.img.at(kSize / 4, kSize / 2);
  const auto right = r.img.at(3 * kSize / 4, kSize / 2);
  INFO(
      "left " << scene::rgba_string(left) << " right "
              << scene::rgba_string(right));
  CHECK(left[0] > 200);
  CHECK(left[2] < 50);
  CHECK(right[0] < 50);
  CHECK(right[2] > 200);
}

TEST_CASE(
    "a scene without an environment has no ambient light",
    "[gfx][scene][environment]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "scenepp.vert", kVert);
  const QString fs = writeText(dir, "scenepp_env_scaled.frag", kEnvAmbientScaledFrag);

  Result r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster
        = sceneChain(p, stateWith({nodeWith(quad(-1, 1, 0xF1A0021u), 21)}), vs, fs);
    if(raster < 0)
    {
      r.err = "chain build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.err = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(5);
    r.img = p.readback(sink);
  });
  if(r.skipped)
    SKIP("backend unavailable");
  REQUIRE(r.err.empty());
  REQUIRE(r.img.valid());
  const auto c = r.img.center();
  INFO("center " << scene::rgba_string(c));
  CHECK(int(c[0]) == 0);
  CHECK(int(c[1]) == 0);
  CHECK(int(c[2]) == 0);
  CHECK(int(c[3]) == 255);
}

TEST_CASE(
    "alpha-blended draws composite over the opaque and blended draws behind them",
    "[gfx][scene][blend]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "scenepp_depth.vert", kDepthVert);
  const QString fs = writeText(dir, "scenepp_blend.frag", kBlendFrag);

  // Quad over x in [x0, x1] at world depth z; the default camera looks down
  // -Z from z = 3, and the vertex shader writes z as reverse-Z depth, so a
  // larger z is nearer in both.
  auto mats = std::make_shared<std::vector<ossia::material_component_ptr>>();
  const auto quadAt = [&](float x0, float x1, float z, uint64_t id,
                          std::array<float, 4> color, bool blend) {
    auto pos = cpuBuffer(
        {x0, -1, z, x1, -1, z, x1, 1, z, x0, -1, z, x1, 1, z, x0, 1, z},
        ossia::buffer_data::usage::vertex_buffer);
    auto mat = std::make_shared<ossia::material_component>();
    mat->stable_id = id + 0x100u;
    mat->unlit = true;
    std::copy(color.begin(), color.end(), mat->base_color_factor);
    mat->alpha = blend ? ossia::alpha_mode::blend : ossia::alpha_mode::opaque_;
    mats->push_back(mat);
    ossia::mesh_primitive prim;
    prim.vertex_buffers = {pos};
    ossia::vertex_attribute p;
    p.semantic = ossia::attribute_semantic::position;
    p.format = ossia::vertex_format::float3;
    p.buffer_index = 0;
    p.byte_stride = 12;
    prim.attributes.push_back(p);
    prim.topology = ossia::primitive_topology::triangles;
    prim.vertex_count = 6;
    prim.stable_id = id;
    prim.bounds = {{x0, -1.f, z}, {x1, 1.f, z}};
    prim.material = mat;
    auto mesh = std::make_shared<ossia::mesh_component>();
    mesh->primitives.push_back(std::move(prim));
    mesh->bounds = mesh->primitives[0].bounds;
    mesh->dirty_index = 1;
    return nodeWith(ossia::mesh_component_ptr(std::move(mesh)), id);
  };

  // Scene order is the worst case for drawing in order: blended before
  // opaque, and the nearer blended quad before the farther one.
  //  left:  opaque blue at z = 0.1 behind 50% red at z = 0.5
  //  right: 50% red at z = 0.6 in front of 50% green at z = 0.3
  auto st = stateWith(
      {quadAt(-1, 0, 0.5f, 0xF1A0051u, {1, 0, 0, 0.5f}, true),
       quadAt(0, 1, 0.6f, 0xF1A0052u, {1, 0, 0, 0.5f}, true),
       quadAt(0, 1, 0.3f, 0xF1A0053u, {0, 1, 0, 0.5f}, true),
       quadAt(-1, 0, 0.1f, 0xF1A0054u, {0, 0, 1, 1}, false)});
  st->materials = mats;

  Result r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = sceneChain(p, st, vs, fs);
    if(raster < 0)
    {
      r.err = "chain build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.err = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(5);
    r.img = p.readback(sink);
  });
  if(r.skipped)
    SKIP("backend unavailable");
  REQUIRE(r.err.empty());
  REQUIRE(r.img.valid());
  const auto left = r.img.at(kSize / 4, kSize / 2);
  const auto right = r.img.at(3 * kSize / 4, kSize / 2);
  INFO(
      "left " << scene::rgba_string(left) << " right "
              << scene::rgba_string(right));
  // Red over blue: (0.5, 0, 0.5).
  CHECK(std::abs(int(left[0]) - 128) < 10);
  CHECK(int(left[1]) < 10);
  CHECK(std::abs(int(left[2]) - 128) < 10);
  // Red over green over the black clear: (0.5, 0.25, 0).
  CHECK(std::abs(int(right[0]) - 128) < 10);
  CHECK(std::abs(int(right[1]) - 64) < 10);
  CHECK(int(right[2]) < 10);
}

TEST_CASE("a scene node with visible == false is not drawn", "[gfx][scene][visibility]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "scenepp.vert", kVert);
  const QString fs = writeText(dir, "scenepp_solid.frag", kSolidFrag);

  auto root = std::make_shared<ossia::scene_node>();
  root->id.value = 30;
  {
    auto children = std::make_shared<std::vector<ossia::scene_payload>>();
    children->push_back(nodeWith(quad(-1, 0, 0xF1A0031u), 31, true));
    children->push_back(nodeWith(quad(0, 1, 0xF1A0032u), 32, false));
    root->children = std::move(children);
  }

  Result r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = sceneChain(p, stateWith({root}), vs, fs);
    if(raster < 0)
    {
      r.err = "chain build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.err = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(5);
    r.img = p.readback(sink);
  });
  if(r.skipped)
    SKIP("backend unavailable");
  REQUIRE(r.err.empty());
  REQUIRE(r.img.valid());
  const auto shown = r.img.at(kSize / 4, kSize / 2);
  const auto hidden = r.img.at(3 * kSize / 4, kSize / 2);
  INFO(
      "visible half " << scene::rgba_string(shown) << " hidden half "
                      << scene::rgba_string(hidden));
  CHECK(shown[0] > 240);
  CHECK(hidden[0] < 20);
  CHECK(hidden[1] < 20);
  CHECK(hidden[2] < 20);
}

TEST_CASE(
    "a primitive-cloud bucket attribute's length() is the primitive count",
    "[gfx][scene][primitivecloud]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString csf = writeText(dir, "scenepp_length.cs", kLengthCsf);

  constexpr int kRows = 5;
  auto cloud = std::make_shared<ossia::primitive_cloud_component>();
  {
    std::vector<float> rows(kRows * 4);
    for(int i = 0; i < kRows; ++i)
      rows[i * 4] = float(i);
    cloud->raw_data = cpuBuffer(std::move(rows), ossia::buffer_data::usage::storage_buffer);
  }
  cloud->row_stride = 16;
  cloud->primitive_count = kRows;
  cloud->format_id = "scenepp.cloud";
  cloud->bounds = {{0.f, 0.f, 0.f}, {4.f, 0.f, 0.f}};
  cloud->stable_id = 0xF1A0041u;

  Result r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int hn = p.addNode(std::make_unique<StaticSceneNode>(
        stateWith({nodeWith(ossia::primitive_cloud_component_ptr{cloud}, 41)})));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    auto filterNode = std::make_unique<score::gfx::FlattenedSceneFilterNode>();
    filterNode->m_mode = 12;
    filterNode->m_match = 0;
    filterNode->m_match_str = "scenepp.cloud";
    const int filter = p.addNode(std::move(filterNode));
    const int c = p.addCsf(csf);
    if(hn < 0 || flat < 0 || filter < 0 || c < 0)
    {
      r.err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(hn, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.nodeGeometryIn(filter, 0));
    p.wire(p.nodeGeometryOut(filter, 0), p.geometryIn(c, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(c, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.err = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(5);
    r.img = p.readback(sink);
  });
  if(r.skipped)
    SKIP("backend unavailable");
  // Needs the GL context the render created: never before it.
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  REQUIRE(r.err.empty());
  REQUIRE(r.img.valid());
  const auto px = r.img.center();
  INFO("cloud_id.length() " << int(px[0]));
  CHECK(int(px[0]) == kRows);
}

TEST_CASE(
    "moving a primitive cloud re-uploads its cloud_meta, not its rows",
    "[gfx][scene][primitivecloud]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString csf = writeText(dir, "scenepp_cloud_meta.cs", kCloudMetaCsf);

  constexpr int kRows = 1000;
  auto cloud = std::make_shared<ossia::primitive_cloud_component>();
  {
    std::vector<float> rows(kRows * 4);
    for(int i = 0; i < kRows; ++i)
      rows[i * 4] = float(i + 3); // row 1 starts at float 4: 4.0
    cloud->raw_data = cpuBuffer(std::move(rows), ossia::buffer_data::usage::storage_buffer);
  }
  cloud->row_stride = 16;
  cloud->primitive_count = kRows;
  cloud->format_id = "scenepp.moving";
  cloud->bounds = {{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}};
  cloud->stable_id = 0xF1A0061u;

  const auto placed = [&](float x, int64_t version) {
    ossia::scene_transform t;
    t.translation[0] = x;
    auto children = std::make_shared<std::vector<ossia::scene_payload>>();
    children->push_back(t);
    children->push_back(ossia::primitive_cloud_component_ptr{cloud});
    auto n = std::make_shared<ossia::scene_node>();
    n->id.value = 61;
    n->children = std::move(children);
    auto st = stateWith({n});
    st->version = version;
    st->dirty_index = version;
    return st;
  };

  struct Uploads
  {
    uint64_t rows{}, meta{};
  };
  const auto uploads = [](GfxPipeline& p) {
    Uploads u;
    for(const auto& rl : p.graph().renderLists())
    {
      u.rows += rl->registry().primitiveCloudUploads.rows;
      u.meta += rl->registry().primitiveCloudUploads.meta;
    }
    return u;
  };

  Result before, after;
  Uploads u0, u1, u2;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto sceneNode = std::make_unique<StaticSceneNode>(placed(0.25f, 1));
    auto* scene = sceneNode.get();
    const int hn = p.addNode(std::move(sceneNode));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    auto filterNode = std::make_unique<score::gfx::FlattenedSceneFilterNode>();
    filterNode->m_mode = 12;
    filterNode->m_match = 0;
    filterNode->m_match_str = "scenepp.moving";
    const int filter = p.addNode(std::move(filterNode));
    const int c = p.addCsf(csf);
    if(hn < 0 || flat < 0 || filter < 0 || c < 0)
    {
      before.err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(hn, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.nodeGeometryIn(filter, 0));
    p.wire(p.nodeGeometryOut(filter, 0), p.geometryIn(c, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(c, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      before.skipped = p.skipped();
      before.err = before.skipped ? std::string{} : p.error();
      return;
    }
    p.render(5);
    before.img = p.readback(sink);
    u0 = uploads(p);

    // The cloud moves, a frame at a time, as when a position is automated.
    for(int step = 1; step <= 4; ++step)
    {
      scene->state = placed(0.25f + 0.125f * step, 1 + step);
      p.render(2);
    }
    u1 = uploads(p);
    after.img = p.readback(sink);
    p.render(3);
    u2 = uploads(p);
  });
  if(before.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  REQUIRE(before.err.empty());
  REQUIRE(before.img.valid());
  REQUIRE(after.img.valid());

  const auto px0 = before.img.center();
  const auto px1 = after.img.center();
  INFO("before " << scene::rgba_string(px0) << " after " << scene::rgba_string(px1));
  INFO("row uploads " << u0.rows << " -> " << u1.rows << " -> " << u2.rows
                      << ", meta uploads " << u0.meta << " -> " << u1.meta << " -> "
                      << u2.meta);
  // The consumer reads the rows and the current model matrix.
  CHECK(std::abs(int(px0[0]) - 64) <= 1);  // 0.25
  CHECK(std::abs(int(px1[0]) - 191) <= 1); // 0.75
  CHECK(int(px0[1]) == 4);
  CHECK(int(px1[1]) == 4);
  // The rows went up once; each move re-uploaded cloud_meta, nothing when still.
  CHECK(u0.rows >= 1);
  CHECK(u1.rows == u0.rows);
  CHECK(u1.meta >= u0.meta + 4);
  CHECK(u2.rows == u1.rows);
  CHECK(u2.meta == u1.meta);
}

TEST_CASE(
    "a preprocessor keeps its texture when another one on the same output "
    "grows the shared texture pool",
    "[gfx][scene][material][texture]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const QString kFullFrag = QStringLiteral("presets/rasterizers/classic_pbr_full.frag");
  const QString presetDir = library::dir_of(kFullFrag);
  if(presetDir.isEmpty())
    SKIP(library::skip_reason(kFullFrag));
  const QDir presets(presetDir);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString combine = writeText(dir, "scenepp_side.fs", kSideBySide);
  const QString vs = presets.filePath(QStringLiteral("classic_pbr_full.vert"));
  const QString fs = presets.filePath(QStringLiteral("classic_pbr_full.frag"));

  Result before, after;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    const library::RootGuard libraryRoot{ctx};

    GfxPipeline p;
    auto nodeA = std::make_unique<StaticSceneNode>(
        texturedQuad(solidTextured(Qt::red, 0xF1A0051u), 0xF1A0052u, 1));
    auto nodeB = std::make_unique<StaticSceneNode>();
    auto matB = std::make_shared<ossia::material_component>();
    matB->stable_id = 0xF1A0053u;
    matB->unlit = true;
    nodeB->state = texturedQuad(matB, 0xF1A0054u, 1);
    auto* sceneB = nodeB.get();

    const int ha = p.addNode(std::move(nodeA));
    const int hb = p.addNode(std::move(nodeB));
    const int fa = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int fb = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int ra = p.addRaster(vs, fs);
    const int rb = p.addRaster(vs, fs);
    const int side = p.addIsf(combine);
    if(ha < 0 || hb < 0 || fa < 0 || fb < 0 || ra < 0 || rb < 0 || side < 0)
    {
      before.err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(ha, 0), p.nodeSceneIn(fa, 0));
    p.wire(p.nodeSceneOut(hb, 0), p.nodeSceneIn(fb, 0));
    p.wire(p.nodeGeometryOut(fa, 0), p.geometryIn(ra, 0));
    p.wire(p.nodeGeometryOut(fb, 0), p.geometryIn(rb, 0));
    p.wire(p.imageOut(ra, 0), p.imageIn(side, 0));
    p.wire(p.imageOut(rb, 0), p.imageIn(side, 1));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(side, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      before.skipped = p.skipped();
      before.err = before.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    before.img = p.readback(sink);

    // B now references a texture of the same size: a second layer in the
    // bucket A's texture lives in, so the bucket's array is recreated.
    sceneB->state = texturedQuad(solidTextured(Qt::blue, 0xF1A0055u), 0xF1A0054u, 2);
    p.render(6);
    after.img = p.readback(sink);
  });
  if(before.skipped)
    SKIP("backend unavailable");
  REQUIRE(before.err.empty());
  REQUIRE(before.img.valid());
  REQUIRE(after.img.valid());

  const int y = kSize / 2;
  const auto a0 = before.img.at(20, y);
  const auto b0 = before.img.at(44, y);
  const auto a1 = after.img.at(20, y);
  const auto b1 = after.img.at(44, y);
  INFO(
      "before A " << scene::rgba_string(a0) << " B " << scene::rgba_string(b0)
                  << " | after A " << scene::rgba_string(a1) << " B "
                  << scene::rgba_string(b1));
  const auto red = [](auto c) { return c[0] > 200 && c[1] < 50 && c[2] < 50; };
  const auto blue = [](auto c) { return c[2] > 200 && c[0] < 50 && c[1] < 50; };
  const auto white = [](auto c) { return c[0] > 200 && c[1] > 200 && c[2] > 200; };
  CHECK(red(a0));
  CHECK(white(b0));
  CHECK(red(a1));
  CHECK(blue(b1));
}

TEST_CASE(
    "a scene parsed again from the same file keeps its textures",
    "[gfx][scene][material][texture]")
{
  // An asset loaded again (the same file chosen again, or redo of a file
  // change) brings fresh material objects carrying the stable ids of the ones
  // they replace. They get fresh Material arena slots, which must be filled
  // although the materials list looks unchanged.
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const QString kFullFrag = QStringLiteral("presets/rasterizers/classic_pbr_full.frag");
  const QString presetDir = library::dir_of(kFullFrag);
  if(presetDir.isEmpty())
    SKIP(library::skip_reason(kFullFrag));
  const QDir presets(presetDir);
  const QString vs = presets.filePath(QStringLiteral("classic_pbr_full.vert"));
  const QString fs = presets.filePath(QStringLiteral("classic_pbr_full.frag"));

  Result before, after;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    const library::RootGuard libraryRoot{ctx};

    GfxPipeline p;
    auto node = std::make_unique<StaticSceneNode>(
        texturedQuad(solidTextured(Qt::yellow, 0xD0C0001u), 0xD0C0002u, 1));
    auto* scene = node.get();
    const int h = p.addNode(std::move(node));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(vs, fs);
    if(h < 0 || flat < 0 || raster < 0)
    {
      before.err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(h, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      before.skipped = p.skipped();
      before.err = before.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    before.img = p.readback(sink);

    // The same file parsed again: equal ids, new objects.
    scene->state
        = texturedQuad(solidTextured(Qt::yellow, 0xD0C0001u), 0xD0C0002u, 2);
    p.render(6);
    after.img = p.readback(sink);
  });
  if(before.skipped)
    SKIP("backend unavailable");
  REQUIRE(before.err.empty());
  REQUIRE(before.img.valid());
  REQUIRE(after.img.valid());

  const auto c0 = before.img.at(kSize / 2, kSize / 2);
  const auto c1 = after.img.at(kSize / 2, kSize / 2);
  INFO("before " << scene::rgba_string(c0) << " after " << scene::rgba_string(c1));
  const auto yellow = [](auto c) { return c[0] > 200 && c[1] > 200 && c[2] < 50; };
  CHECK(yellow(c0));
  CHECK(yellow(c1));
}
