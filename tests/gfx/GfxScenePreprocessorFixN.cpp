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
//  - Two preprocessors share the registry's texture pool: when the second one
//    adds a texture, the first one's layer is kept, and when that grows a
//    bucket (a new QRhiTexture array) the first one republishes, so its
//    classic_pbr_full consumer keeps sampling its own texture from a live
//    array instead of the deleted one.

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/FlattenedSceneFilterNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>
#include <Library/LibrarySettings.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <cstring>

using namespace score::test::gfx;

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

#define FIXN_RASTER_HEADER                                                         \
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

constexpr const char* kMaterialFrag = "/*{" FIXN_RASTER_HEADER R"(
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

constexpr const char* kEnvFrag = "/*{" FIXN_RASTER_HEADER R"(
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

constexpr const char* kSolidFrag = "/*{" FIXN_RASTER_HEADER R"(
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

struct SceneNode final : score::gfx::ProcessNode
{
  std::shared_ptr<ossia::scene_state> state;
  SceneNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct SceneRenderer final : score::gfx::NodeRenderer
{
  const SceneNode& self;
  ossia::scene_spec m_scene;
  explicit SceneRenderer(const SceneNode& n)
      : NodeRenderer{n}
      , self{n}
  {
  }
  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    m_initialized = true;
  }
  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
  {
    m_scene.state = self.state;
  }
  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      score::gfx::Edge& edge) override
  {
    if(!m_scene.state || !edge.sink || !edge.sink->node)
      return;
    auto rn = edge.sink->node->renderedNodes.find(&renderer);
    if(rn == edge.sink->node->renderedNodes.end())
      return;
    auto& in = edge.sink->node->input;
    auto it = std::find(in.begin(), in.end(), edge.sink);
    if(it != in.end())
      rn->second->process(int(it - in.begin()), m_scene, edge.source);
  }
  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override { }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList&) override
  {
    m_scene = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer* SceneNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new SceneRenderer{*this};
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
  auto node = std::make_unique<SceneNode>();
  node->state = std::move(st);
  const int hn = p.addNode(std::move(node));
  const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
  const int raster = p.addRaster(vs, fs);
  if(hn < 0 || flat < 0 || raster < 0)
    return -1;
  p.wire(p.nodeSceneOut(hn, 0), p.nodeSceneIn(flat, 0));
  p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
  return raster;
}

QString presetDir()
{
  const QString env = qEnvironmentVariable("SCORE_CSF_PRESETS");
  if(!env.isEmpty())
    return env;
  return QDir::homePath()
         + QStringLiteral(
             "/Documents/ossia/score/packages/csf-examples/presets/rasterizers");
}

QString libraryRoot()
{
  QDir root(presetDir());
  return root.cd(QStringLiteral("../../../..")) ? root.absolutePath() : QString{};
}

ossia::material_component_ptr solidTextured(QColor color, uint64_t id)
{
  QImage img(64, 64, QImage::Format_RGBA8888);
  img.fill(color);
  QByteArray png;
  {
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
  }
  auto src = std::make_shared<ossia::texture_source>();
  src->embedded_data = std::make_shared<std::vector<uint8_t>>(
      reinterpret_cast<const uint8_t*>(png.constData()),
      reinterpret_cast<const uint8_t*>(png.constData()) + png.size());
  src->mime_type = "image/png";

  auto m = std::make_shared<ossia::material_component>();
  m->stable_id = id;
  m->unlit = true;
  m->base_color_texture.source = std::move(src);
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

std::string rgb(std::array<uint8_t, 4> c)
{
  return "(" + std::to_string(c[0]) + "," + std::to_string(c[1]) + ","
         + std::to_string(c[2]) + ")";
}
}

TEST_CASE(
    "a mesh without a material reads the white default material after a "
    "RenderList rebuild before the first frame",
    "[gfx][scene][material][fixn]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "fixn.vert", kVert);
  const QString fs = writeText(dir, "fixn_material.frag", kMaterialFrag);

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
  INFO("center " << rgb(c));
  CHECK(c[0] > 240);
  CHECK(c[1] > 240);
  CHECK(c[2] > 240);
}

TEST_CASE(
    "two scene preprocessors on one output each bind their own environment",
    "[gfx][scene][environment][fixn]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "fixn.vert", kVert);
  const QString fs = writeText(dir, "fixn_env.frag", kEnvFrag);
  const QString combine = writeText(dir, "fixn_side.fs", kSideBySide);

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
  INFO("left " << rgb(left) << " right " << rgb(right));
  CHECK(left[0] > 200);
  CHECK(left[2] < 50);
  CHECK(right[0] < 50);
  CHECK(right[2] > 200);
}

TEST_CASE("a scene node with visible == false is not drawn", "[gfx][scene][visibility][fixn]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeText(dir, "fixn.vert", kVert);
  const QString fs = writeText(dir, "fixn_solid.frag", kSolidFrag);

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
  INFO("visible half " << rgb(shown) << " hidden half " << rgb(hidden));
  CHECK(shown[0] > 240);
  CHECK(hidden[0] < 20);
  CHECK(hidden[1] < 20);
  CHECK(hidden[2] < 20);
}

TEST_CASE(
    "a primitive-cloud bucket attribute's length() is the primitive count",
    "[gfx][scene][primitivecloud][fixn]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString csf = writeText(dir, "fixn_length.cs", kLengthCsf);

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
  cloud->format_id = "fixn.cloud";
  cloud->bounds = {{0.f, 0.f, 0.f}, {4.f, 0.f, 0.f}};
  cloud->stable_id = 0xF1A0041u;

  Result r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto node = std::make_unique<SceneNode>();
    node->state = stateWith({nodeWith(ossia::primitive_cloud_component_ptr{cloud}, 41)});
    const int hn = p.addNode(std::move(node));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    auto filterNode = std::make_unique<score::gfx::FlattenedSceneFilterNode>();
    filterNode->m_mode = 12;
    filterNode->m_match = 0;
    filterNode->m_match_str = "fixn.cloud";
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
  REQUIRE(r.err.empty());
  REQUIRE(r.img.valid());
  const auto px = r.img.center();
  INFO("cloud_id.length() " << int(px[0]));
  CHECK(int(px[0]) == kRows);
}

TEST_CASE(
    "a preprocessor keeps its texture when another one on the same output "
    "grows the shared texture pool",
    "[gfx][scene][material][texture][fixn]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const QDir presets(presetDir());
  if(!QFileInfo::exists(presets.filePath("classic_pbr_full.frag")))
    SKIP("preset library not found at " + presetDir().toStdString());
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString combine = writeText(dir, "fixn_side.fs", kSideBySide);
  const QString vs = presets.filePath(QStringLiteral("classic_pbr_full.vert"));
  const QString fs = presets.filePath(QStringLiteral("classic_pbr_full.frag"));

  Result before, after;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto& lib = ctx.settings<Library::Settings::Model>();
    const QString previousRoot = lib.getRootPath();
    lib.setRootPath(libraryRoot());
    struct Restore
    {
      Library::Settings::Model& lib;
      QString root;
      ~Restore() { lib.setRootPath(root); }
    } restore{lib, previousRoot};

    GfxPipeline p;
    auto nodeA = std::make_unique<SceneNode>();
    nodeA->state = texturedQuad(solidTextured(Qt::red, 0xF1A0051u), 0xF1A0052u, 1);
    auto nodeB = std::make_unique<SceneNode>();
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
  INFO("before A " << rgb(a0) << " B " << rgb(b0) << " | after A " << rgb(a1) << " B "
                   << rgb(b1));
  const auto red = [](auto c) { return c[0] > 200 && c[1] < 50 && c[2] < 50; };
  const auto blue = [](auto c) { return c[2] > 200 && c[0] < 50 && c[1] < 50; };
  const auto white = [](auto c) { return c[0] > 200 && c[1] > 200 && c[2] > 200; };
  CHECK(red(a0));
  CHECK(white(b0));
  CHECK(red(a1));
  CHECK(blue(b1));
}
