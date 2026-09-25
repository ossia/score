// Texture inlet settings on the avnd inlets of Texture to buffer and Texture
// Info.
//
// A texture inlet is a render target: its size and format come from the
// inlet's settings, the render size of the graph when no size is set. The
// upstream's own texture is read directly only when the inlet has a single
// cable and no settings of its own (no size, default RGBA8 format), so a
// size or format the user set is what the node sees, and several cables are
// always mixed in the render target.
//
// Registration: see the test_gfx_inlet_settings_i6 target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Threedim/BufferInfo.hpp>
#include <Threedim/TextureInfo.hpp>
#include <Threedim/TextureToBuffer.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
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

score::gfx::Port* imageInput(score::gfx::Node& n)
{
  for(auto* port : n.input)
    if(port->type == score::gfx::Types::Image)
      return port;
  return nullptr;
}

score::gfx::Port* bufferInput(score::gfx::Node& n)
{
  for(auto* port : n.input)
    if(port->type == score::gfx::Types::Buffer)
      return port;
  return nullptr;
}

ossia::render_target_spec
inletSpec(std::optional<ossia::texture_size> sz, ossia::texture_format fmt)
{
  ossia::render_target_spec s;
  s.size = sz;
  s.format = fmt;
  s.mag_filter = ossia::texture_filter::NEAREST;
  s.min_filter = ossia::texture_filter::NEAREST;
  s.address_u = ossia::texture_address_mode::CLAMP_TO_EDGE;
  s.address_v = ossia::texture_address_mode::CLAMP_TO_EDGE;
  s.address_w = ossia::texture_address_mode::CLAMP_TO_EDGE;
  return s;
}

struct Producer
{
  const char* path;
  bool compute;
};

struct T2B
{
  bool skipped{};
  std::string error;
  int width{}, height{};
  halp::custom_variable_texture::texture_format format{};
  int64_t byteSize{};
  std::vector<unsigned char> bytes;
};

// producers -> Texture to buffer -> Buffer Info (the output node).
T2B runTextureToBuffer(
    score::gfx::GraphicsApi api, std::vector<Producer> producers,
    std::optional<ossia::render_target_spec> spec,
    std::optional<ossia::render_target_spec> later = std::nullopt)
{
  T2B out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    const auto& ctx = doc->context();
    HalpProcesses procs;
    GfxPipeline p;

    std::vector<int> prods;
    for(auto& pr : producers)
    {
      const int idx = pr.compute ? p.addCsf(corpus(pr.path)) : p.addIsf(corpus(pr.path));
      if(idx < 0)
      {
        out.error = "producer build failed: " + p.error();
        return;
      }
      prods.push_back(idx);
    }

    auto t2bOwned = procs.make<Threedim::TextureToBuffer>(ctx);
    auto* t2bNode = static_cast<oscr::GfxNode<Threedim::TextureToBuffer>*>(t2bOwned.get());
    const int t2b = p.addNode(std::move(t2bOwned));
    auto infoOwned = procs.make<Threedim::BufferInfo>(ctx);
    auto* info = static_cast<score::gfx::OutputNode*>(infoOwned.get());
    const int bi = p.addNode(std::move(infoOwned));

    auto* tin = imageInput(*p.node(t2b));
    REQUIRE(tin);
    if(spec)
      p.node(t2b)->process(0, *spec);
    for(int idx : prods)
      p.wire(p.imageOut(idx, 0), tin);
    p.wire(p.nodeBufferOut(t2b, 0), bufferInput(*p.node(bi)));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    for(int f = 0; f < 6; ++f)
    {
      p.render(1);
      info->render();
    }
    if(later)
    {
      p.node(t2b)->process(0, *later);
      for(int f = 0; f < 6; ++f)
      {
        p.render(1);
        info->render();
      }
    }

    REQUIRE(!t2bNode->renderedNodes.empty());
    auto* rn = dynamic_cast<oscr::GfxRenderer<Threedim::TextureToBuffer>*>(
        t2bNode->renderedNodes.begin()->second);
    REQUIRE(rn);
    auto& st = rn->state;
    const auto& t = st->inputs.texture.texture;
    out.width = t.width;
    out.height = t.height;
    out.format = t.format;
    out.byteSize = st->outputs.buffer.buffer.byte_size;
    if(t.bytes && t.bytesize() > 0)
      out.bytes.assign(t.bytes, t.bytes + t.bytesize());
  });
  return out;
}

struct Info
{
  bool skipped{};
  std::string error;
  int width{}, height{};
  std::string format;
};

// producer -> Texture Info (the output node).
Info runTextureInfo(
    score::gfx::GraphicsApi api, Producer producer,
    std::optional<ossia::render_target_spec> spec)
{
  Info out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    const auto& ctx = doc->context();
    HalpProcesses procs;
    GfxPipeline p;

    const int prod = producer.compute ? p.addCsf(corpus(producer.path))
                                      : p.addIsf(corpus(producer.path));
    if(prod < 0)
    {
      out.error = "producer build failed: " + p.error();
      return;
    }
    auto infoOwned = procs.make<Threedim::TextureInfo>(ctx);
    auto* infoNode = infoOwned.get();
    auto* info = static_cast<score::gfx::OutputNode*>(infoOwned.get());
    const int ti = p.addNode(std::move(infoOwned));
    if(spec)
      p.node(ti)->process(0, *spec);
    p.wire(p.imageOut(prod, 0), imageInput(*p.node(ti)));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    for(int f = 0; f < 6; ++f)
    {
      p.render(1);
      info->render();
    }

    REQUIRE(!infoNode->renderedNodes.empty());
    auto* rn = dynamic_cast<oscr::GfxRenderer<Threedim::TextureInfo>*>(
        infoNode->renderedNodes.begin()->second);
    REQUIRE(rn);
    out.width = rn->state->outputs.width.value;
    out.height = rn->state->outputs.height.value;
    out.format = rn->state->outputs.format.value;
  });
  return out;
}

std::array<float, 4> texelF(const T2B& r, int x, int y)
{
  std::array<float, 4> px{};
  const std::size_t off = (std::size_t(y) * r.width + x) * 16;
  if(off + 16 <= r.bytes.size())
    std::memcpy(px.data(), r.bytes.data() + off, 16);
  return px;
}

std::array<int, 4> texel8(const T2B& r, int x, int y)
{
  std::array<int, 4> px{-1, -1, -1, -1};
  const std::size_t off = (std::size_t(y) * r.width + x) * 4;
  if(off + 4 <= r.bytes.size())
    for(int i = 0; i < 4; ++i)
      px[i] = r.bytes[off + i];
  return px;
}
}

TEST_CASE(
    "Texture to buffer reads its inlet at the size and format set on it",
    "[gfx][avnd][texture][inlet-settings]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const T2B isf = runTextureToBuffer(
      api, {{"isf-solid-color.fs", false}},
      inletSpec(ossia::texture_size{24, 16}, ossia::texture_format::RGBA32F));
  if(isf.skipped)
    SKIP("backend unavailable");
  INFO("error=" << isf.error);
  REQUIRE(isf.error.empty());
  CHECK(isf.width == 24);
  CHECK(isf.height == 16);
  CHECK(isf.format == halp::custom_variable_texture::RGBA32F);
  CHECK(isf.byteSize == 24 * 16 * 16);
  const auto c = texelF(isf, 12, 8);
  CHECK(c[0] > 0.99f);
  CHECK(c[1] < 0.01f);
  CHECK(c[2] > 0.99f);

  const T2B csf = runTextureToBuffer(
      api, {{"csf-image-rgba16f.cs", true}},
      inletSpec(ossia::texture_size{24, 16}, ossia::texture_format::RGBA32F));
  INFO("error=" << csf.error);
  REQUIRE(csf.error.empty());
  CHECK(csf.width == 24);
  CHECK(csf.height == 16);
  CHECK(csf.format == halp::custom_variable_texture::RGBA32F);
  CHECK(csf.byteSize == 24 * 16 * 16);

  const T2B fmtOnly = runTextureToBuffer(
      api, {{"csf-image-rgba16f.cs", true}},
      inletSpec(std::nullopt, ossia::texture_format::RGBA32F));
  INFO("error=" << fmtOnly.error);
  REQUIRE(fmtOnly.error.empty());
  CHECK(fmtOnly.format == halp::custom_variable_texture::RGBA32F);
  CHECK(fmtOnly.width == oscr::CustomGpuOutputNodeBase::defaultRenderSize.width());
  CHECK(fmtOnly.byteSize == int64_t(fmtOnly.width) * fmtOnly.height * 16);
}

TEST_CASE(
    "Texture to buffer follows inlet settings changed while running",
    "[gfx][avnd][texture][inlet-settings]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const T2B r = runTextureToBuffer(
      api, {{"isf-solid-color.fs", false}},
      inletSpec(ossia::texture_size{24, 16}, ossia::texture_format::RGBA8),
      inletSpec(ossia::texture_size{10, 6}, ossia::texture_format::RGBA32F));
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  CHECK(r.width == 10);
  CHECK(r.height == 6);
  CHECK(r.format == halp::custom_variable_texture::RGBA32F);
  CHECK(r.byteSize == 10 * 6 * 16);

  const T2B csf = runTextureToBuffer(
      api, {{"csf-image-rgba16f.cs", true}}, std::nullopt,
      inletSpec(ossia::texture_size{10, 6}, ossia::texture_format::RGBA32F));
  INFO("error=" << csf.error);
  REQUIRE(csf.error.empty());
  CHECK(csf.width == 10);
  CHECK(csf.height == 6);
  CHECK(csf.format == halp::custom_variable_texture::RGBA32F);
  CHECK(csf.byteSize == 10 * 6 * 16);
}

TEST_CASE(
    "Texture to buffer without inlet settings renders at the graph's render size",
    "[gfx][avnd][texture][inlet-settings]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const T2B r = runTextureToBuffer(api, {{"isf-solid-color.fs", false}}, std::nullopt);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  const auto sz = oscr::CustomGpuOutputNodeBase::defaultRenderSize;
  CHECK(r.width == sz.width());
  CHECK(r.height == sz.height());
  CHECK(r.format == halp::custom_variable_texture::RGBA8);
  CHECK(r.byteSize == int64_t(sz.width()) * sz.height() * 4);

  const T2B csf = runTextureToBuffer(api, {{"csf-image-rgba16f.cs", true}}, std::nullopt);
  INFO("error=" << csf.error);
  REQUIRE(csf.error.empty());
  CHECK(csf.format == halp::custom_variable_texture::RGBA16F);
}

TEST_CASE(
    "Texture to buffer mixes every cable into its inlet",
    "[gfx][avnd][texture][inlet-settings]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const T2B r = runTextureToBuffer(
      api, {{"i6-half-left.fs", false}, {"i6-half-right.fs", false}},
      inletSpec(ossia::texture_size{16, 8}, ossia::texture_format::RGBA8));
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.width == 16);
  REQUIRE(r.height == 8);
  const auto left = texel8(r, 2, 4);
  const auto right = texel8(r, 13, 4);
  INFO("left " << left[0] << "," << left[1] << "," << left[2] << " right " << right[0]
               << "," << right[1] << "," << right[2]);
  CHECK(left[0] == 255);
  CHECK(left[1] == 0);
  CHECK(right[0] == 0);
  CHECK(right[1] == 255);

  const T2B two = runTextureToBuffer(
      api, {{"csf-image-rgba16f.cs", true}, {"csf-image-rgba16f.cs", true}},
      std::nullopt);
  INFO("error=" << two.error);
  REQUIRE(two.error.empty());
  CHECK(two.format == halp::custom_variable_texture::RGBA8);
  CHECK(two.byteSize == int64_t(two.width) * two.height * 4);
}

TEST_CASE(
    "Texture Info reports the inlet's size and format",
    "[gfx][avnd][texture][inlet-settings]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Info isf = runTextureInfo(
      api, {"isf-solid-color.fs", false},
      inletSpec(ossia::texture_size{24, 16}, ossia::texture_format::RGBA16F));
  if(isf.skipped)
    SKIP("backend unavailable");
  INFO("error=" << isf.error);
  REQUIRE(isf.error.empty());
  CHECK(isf.width == 24);
  CHECK(isf.height == 16);
  CHECK(isf.format == "RGBA16F");

  const Info csf = runTextureInfo(
      api, {"csf-image-rgba16f.cs", true},
      inletSpec(ossia::texture_size{24, 16}, ossia::texture_format::RGBA32F));
  INFO("error=" << csf.error);
  REQUIRE(csf.error.empty());
  CHECK(csf.width == 24);
  CHECK(csf.height == 16);
  CHECK(csf.format == "RGBA32F");

  const Info unset = runTextureInfo(api, {"csf-image-rgba16f.cs", true}, std::nullopt);
  INFO("error=" << unset.error);
  REQUIRE(unset.error.empty());
  CHECK(unset.format == "RGBA16F");

  const Info isfUnset = runTextureInfo(api, {"isf-solid-color.fs", false}, std::nullopt);
  INFO("error=" << isfUnset.error);
  REQUIRE(isfUnset.error.empty());
  CHECK(isfUnset.width == oscr::CustomGpuOutputNodeBase::defaultRenderSize.width());
  CHECK(isfUnset.height == oscr::CustomGpuOutputNodeBase::defaultRenderSize.height());
  CHECK(isfUnset.format == "RGBA8");
}
