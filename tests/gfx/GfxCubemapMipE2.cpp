// Pins textureLod on an ISF cubemap input fed a mipmapped cube (the shape of
// a Cubemap Loader / Cubemap Composer cable into cubemap_view): the mip level
// argument reaches the cube's mip chain. The input sampler had no mipmap mode,
// which clamps every lookup to level 0.
//
// A halp object publishes a 4x4 cube with three mips, every face of level 0
// red, level 1 green, level 2 blue.
//
// Registration: see the test_gfx_cubemap_mip_e2 target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct MippedCube
{
  halp_meta(name, "Mipped cube")
  halp_meta(c_name, "test_e2_mipped_cube")
  halp_meta(category, "Test")
  halp_meta(uuid, "0f3b8a52-61d4-4c7e-b2a9-5e7d13c4f8a6")

  struct
  {
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Texture"> texture;
  } outputs;

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) { }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge*)
  {
    if(m_tex)
      return;
    m_tex = renderer.state.rhi->newTexture(
        QRhiTexture::RGBA8, QSize{4, 4}, 1,
        QRhiTexture::CubeMap | QRhiTexture::MipMapped);
    m_tex->create();

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
            QRhiTextureSubresourceUploadDescription{px.data(), quint32(px.size())}});
    }
    QRhiTextureUploadDescription desc;
    desc.setEntries(entries.begin(), entries.end());
    res.uploadTexture(m_tex, desc);

    outputs.texture.texture.handle = m_tex;
    outputs.texture.texture.width = 4;
    outputs.texture.texture.height = 4;
    outputs.texture.texture.format = halp::gpu_texture::RGBA8;
  }

  void release(score::gfx::RenderList&)
  {
    delete m_tex;
    m_tex = nullptr;
    outputs.texture.texture.handle = nullptr;
  }

  void runInitialPasses(
      score::gfx::RenderList&, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      score::gfx::Edge&)
  {
  }

  void operator()() { }

  QRhiTexture* m_tex{};
};

QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}
}

TEST_CASE(
    "textureLod on an ISF cubemap input reads the requested mip",
    "[gfx][avnd][cubemap][mip]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const int mip = GENERATE(0, 1, 2);
  CAPTURE(backend_name(api), mip);

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      err = "no document";
      return;
    }
    const auto& ctx = doc->context();
    auto model = std::make_unique<oscr::ProcessModel<MippedCube>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));

    GfxPipeline p;
    const int producer = p.addNode(std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<MippedCube>{*raw, {}, Gfx::exec_controls{}, 1, ctx}});
    const int view = p.addIsf(corpus("e2-cube-lod.fs"));
    if(producer < 0 || view < 0)
    {
      err = "node build failed: " + p.error();
      return;
    }
    p.wire(p.nodeImageOut(producer, 0), p.imageIn(view, 0));
    const int sink = p.addSink({8, 8});
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    setControl(*p.isf(view), nth_control_input(*p.isf(view), 0), float(mip));
    p.render(4);
    img = p.readback(sink);
    if(!img.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << err);
  REQUIRE(err.empty());
  const auto px = img.center();
  CAPTURE(int(px[0]), int(px[1]), int(px[2]));
  CHECK(int(px[0]) == (mip == 0 ? 255 : 0));
  CHECK(int(px[1]) == (mip == 1 ? 255 : 0));
  CHECK(int(px[2]) == (mip == 2 ? 255 : 0));
}
