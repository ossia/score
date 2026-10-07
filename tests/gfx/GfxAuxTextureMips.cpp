// An AUXILIARY texture whose declaration leaves MIPMAP_MODE out reads the mip
// chain of the texture it is bound to. The scene's prefiltered_map is the case
// that matters: classic_pbr_openpbr reads it with textureLod(…, roughness *
// maxMip) and declares no sampler, so with a mip-less sampler every roughness
// got the mirror level.
//
// A scene producer stamps a 4x4 three-mip cube as environment.prefiltered_map,
// level 0 red, level 1 green, level 2 blue on every face. ScenePreprocessor
// publishes it on its geometry and a raw raster declaring it in AUXILIARY
// samples it with textureLod at a fixed level.
#include "GfxSceneSource.hpp"
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
std::shared_ptr<ossia::buffer_resource> fullscreenPositions()
{
  auto owned = std::make_shared<std::vector<float>>(std::vector<float>{
      -1, -1, 0, 1, -1, 0, 1, 1, 0, -1, -1, 0, 1, 1, 0, -1, 1, 0});
  auto res = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(owned, owned->data());
  bd.byte_size = int64_t(owned->size() * sizeof(float));
  bd.usage_hint = ossia::buffer_data::usage::vertex_buffer;
  res->resource = bd;
  res->dirty_index = 1;
  return res;
}

std::shared_ptr<ossia::scene_state> fullscreenScene()
{
  ossia::mesh_primitive prim;
  prim.vertex_buffers = {fullscreenPositions()};
  ossia::vertex_attribute p;
  p.semantic = ossia::attribute_semantic::position;
  p.format = ossia::vertex_format::float3;
  p.buffer_index = 0;
  p.byte_stride = 12;
  prim.attributes.push_back(p);
  prim.topology = ossia::primitive_topology::triangles;
  prim.vertex_count = 6;
  prim.stable_id = 0xA0C0BE1u;
  prim.bounds = {{-1.f, -1.f, 0.f}, {1.f, 1.f, 0.f}};

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->dirty_index = 1;

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(ossia::mesh_component_ptr(std::move(mesh)));
  auto n = std::make_shared<ossia::scene_node>();
  n->id.value = 0xA0C0BE1u;
  n->children = std::move(children);

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>(
      std::vector<ossia::scene_node_ptr>{n});
  st->version = 1;
  st->dirty_index = 1;
  return st;
}

// Publishes fullscreenScene() with a three-mip cube as its prefiltered_map.
struct MippedEnvironmentNode final : score::gfx::ProcessNode
{
  MippedEnvironmentNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override
  {
    struct Renderer final : scene::SourceRenderer
    {
      using SourceRenderer::SourceRenderer;
      QRhiTexture* m_cube{};

      void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
      {
        m_initialized = true;
      }

      void update(
          score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
          score::gfx::Edge*) override
      {
        if(m_cube)
          return;
        m_cube = renderer.state.rhi->newTexture(
            QRhiTexture::RGBA8, QSize{4, 4}, 1,
            QRhiTexture::CubeMap | QRhiTexture::MipMapped);
        m_cube->create();

        static constexpr std::array<std::array<uint8_t, 4>, 3> colours{
            {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}}};
        QList<QRhiTextureUploadEntry> entries;
        std::vector<std::vector<uint8_t>> storage;
        for(int level = 0; level < 3; ++level)
        {
          const int side = 4 >> level;
          auto& px = storage.emplace_back(side * side * 4);
          for(std::size_t i = 0; i < px.size(); i += 4)
            std::copy_n(colours[level].begin(), 4, px.begin() + i);
          for(int face = 0; face < 6; ++face)
            entries.push_back(QRhiTextureUploadEntry{
                face, level,
                QRhiTextureSubresourceUploadDescription(px.data(), px.size())});
        }
        QRhiTextureUploadDescription desc;
        desc.setEntries(entries.begin(), entries.end());
        res.uploadTexture(m_cube, desc);

        auto st = fullscreenScene();
        st->environment.prefiltered_map.native_handle = m_cube;
        m_scene.state = std::move(st);
      }

      void release(score::gfx::RenderList& r) override
      {
        SourceRenderer::release(r);
        delete m_cube;
        m_cube = nullptr;
      }
    };
    return new Renderer{*this};
  }
};

constexpr const char* kVert = R"(void main()
{
  isf_vertShaderInit();
  gl_Position = clipSpaceCorrMatrix * vec4(position.xy, 0.5, 1.0);
  isf_vertShaderFinish();
}
)";

QString writeText(const QTemporaryDir& dir, const QString& name, const QByteArray& text)
{
  const QString path = dir.filePath(name);
  QFile f(path);
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  f.write(text);
  return path;
}

// `sampler` is spliced into the AUXILIARY declaration.
QByteArray lodFrag(int lod, const char* sampler)
{
  return QByteArray(R"(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [ { "TYPE": "vec3", "NAME": "position" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": { "CULL_MODE": "none" },
  "AUXILIARY": [ { "NAME": "prefiltered_map", "TYPE": "cubemap", "VISIBILITY": "fragment")")
         + sampler + R"( } ]
}*/
void main()
{
  isf_FragColor = vec4(textureLod(prefiltered_map, vec3(1.0, 0.0, 0.0), )"
         + QByteArray::number(lod) + R"(.0).rgb, 1.0);
}
)";
}

// The same read from a compute shader, through a geometry input's AUXILIARY.
QByteArray lodCsf(int lod)
{
  return QByteArray(R"(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "8", "HEIGHT": "8" },
    { "NAME": "geoIn", "TYPE": "geometry",
      "ATTRIBUTES": [ { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec3", "ACCESS": "read_only" } ],
      "AUXILIARY": [ { "NAME": "prefiltered_map", "TYPE": "cubemap" } ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  IMG_STORE(outputImage, p, vec4(textureLod(prefiltered_map, vec3(1.0, 0.0, 0.0), )")
         + QByteArray::number(lod) + R"(.0).rgb, 1.0));
}
)";
}

struct Result
{
  bool skipped = false;
  std::string err;
  ReadbackImage img;
};

// csf: read through lodCsf, otherwise through a raw raster declaring `sampler`.
Result renderLod(score::gfx::GraphicsApi api, int lod, const char* sampler, bool csf = false)
{
  Result r;
  QTemporaryDir dir;
  if(!dir.isValid())
  {
    r.err = "no temporary directory";
    return r;
  }
  const QString vs = writeText(dir, "auxmip.vert", kVert);
  const QString fs = writeText(dir, "auxmip.frag", lodFrag(lod, sampler));
  const QString cs = writeText(dir, "auxmip.csf", lodCsf(lod));
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int env = p.addNode(std::make_unique<MippedEnvironmentNode>());
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = csf ? p.addCsf(cs) : p.addRaster(vs, fs);
    if(env < 0 || flat < 0 || raster < 0)
    {
      r.err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(env, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({8, 8});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.err = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(5);
    r.img = p.readback(sink);
    if(!r.img.valid())
      r.err = "empty readback";
  });
  return r;
}
}

TEST_CASE(
    "an AUXILIARY cubemap without MIPMAP_MODE reads the mip textureLod asks for",
    "[gfx][scene][cubemap][mip]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const int lod = GENERATE(0, 1, 2);
  CAPTURE(backend_name(api), lod);

  const auto r = renderLod(api, lod, "");
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.err);
  REQUIRE(r.err.empty());
  const auto px = r.img.center();
  CAPTURE(int(px[0]), int(px[1]), int(px[2]));
  CHECK(int(px[0]) == (lod == 0 ? 255 : 0));
  CHECK(int(px[1]) == (lod == 1 ? 255 : 0));
  CHECK(int(px[2]) == (lod == 2 ? 255 : 0));
}

TEST_CASE(
    "an AUXILIARY cubemap declaring MIPMAP_MODE none keeps reading mip 0",
    "[gfx][scene][cubemap][mip]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto r = renderLod(api, 2, R"(, "MIPMAP_MODE": "none")");
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.err);
  REQUIRE(r.err.empty());
  const auto px = r.img.center();
  CAPTURE(int(px[0]), int(px[1]), int(px[2]));
  CHECK(int(px[0]) == 255);
  CHECK(int(px[1]) == 0);
  CHECK(int(px[2]) == 0);
}

TEST_CASE(
    "a CSF geometry AUXILIARY cubemap without MIPMAP_MODE reads the mip textureLod asks for",
    "[gfx][scene][cubemap][mip][csf]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const int lod = GENERATE(0, 1, 2);
  CAPTURE(backend_name(api), lod);

  const auto r = renderLod(api, lod, "", true);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.err);
  REQUIRE(r.err.empty());
  const auto px = r.img.center();
  CAPTURE(int(px[0]), int(px[1]), int(px[2]));
  CHECK(int(px[0]) == (lod == 0 ? 255 : 0));
  CHECK(int(px[1]) == (lod == 1 ? 255 : 0));
  CHECK(int(px[2]) == (lod == 2 ? 255 : 0));
}
