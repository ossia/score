// Texture inlet settings reaching every avnd texture input (agent L2).
//
// - format_set: an RGBA8 the user picked is a setting, so the avnd bridge
//   renders into the inlet's RGBA8 target instead of reading the upstream's
//   own texture; unset, a single cable still reads the upstream directly.
// - avnd GPU texture inputs publish a sampler built from the inlet's
//   filter / address / mipmap settings, and a mipmapped render target.
// - gpp shader nodes (CustomGpuNode, GpuComputeNode) build their input render
//   targets and samplers from the inlet settings.
// - Gfx::TextureInlet saves and loads the format-set state and the mipmap
//   mode; older documents count a non-RGBA8 format as set. The inspector shows
//   "Auto" for an unset size and format and offers the mipmap mode.
//
// Registration: see the test_gfx_texture_inlet_spec_l2 target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/GpuComputeNode.hpp>
#include <Crousti/GpuNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Gfx/TexturePort.hpp>
#include <Inspector/InspectorLayout.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/dataflow/port.hpp>

#include <Threedim/BufferInfo.hpp>
#include <Threedim/TextureInfo.hpp>
#include <Threedim/TextureToBuffer.hpp>

#include <gpp/commands.hpp>
#include <gpp/layout.hpp>
#include <gpp/meta.hpp>
#include <gpp/ports.hpp>

#include <QComboBox>
#include <QLabel>
#include <QSpinBox>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

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

struct SampledInput
{
  halp_meta(name, "L2 sampled input")
  halp_meta(uuid, "0f3c1d52-4a8e-4b7d-9e21-6c5a2b8d7e43")

  struct layout
  {
    enum
    {
      graphics
    };

    struct fragment_output
    {
      gpp_attribute(0, fragColor, float[4])
      fragColor;
    } fragment_output;

    struct bindings
    {
      gpp::sampler<"tex", 1> tex;
    };
  };

  struct
  {
    gpp::texture_input_port<"In", &layout::bindings::tex> in;
  } inputs;

  struct
  {
    gpp::color_attachment_port<"Out", &layout::fragment_output> out;
  } outputs;

  std::string_view fragment()
  {
    return R"_(
void main() {
  fragColor = texture(tex, vec2(0.5));
}
)_";
  }
};

struct TwoImages
{
  halp_meta(name, "L2 two images")
  halp_meta(uuid, "7d2b9e14-3f6a-4c8b-a5d0-1e9c7b4f2a68")

  struct layout
  {
    halp_meta(local_size_x, 16)
    halp_meta(local_size_y, 16)
    halp_meta(local_size_z, 1)
    halp_flags(compute);

    struct bindings
    {
      struct
      {
        halp_meta(name, "img_a")
        halp_meta(format, "rgba8")
        halp_meta(binding, 0);
        halp_flags(image2D, readonly);
      } a;

      struct
      {
        halp_meta(name, "img_b")
        halp_meta(format, "rgba8")
        halp_meta(binding, 1);
        halp_flags(image2D, readonly);
      } b;
    } bindings;
  };

  using bindings = decltype(layout::bindings);

  struct
  {
    gpp::image_input_port<"A", &bindings::a> a;
    gpp::image_input_port<"B", &bindings::b> b;
  } inputs;

  struct
  {
    struct
    {
      halp_meta(name, "unused")
      float value;
    } unused;
  } outputs;

  std::string_view compute()
  {
    return R"_(
void main() { }
)_";
  }

  gpp::co_dispatch dispatch() { co_return; }
};

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

score::gfx::Port* firstInput(score::gfx::Node& n, score::gfx::Types type)
{
  for(auto* port : n.input)
    if(port->type == type)
      return port;
  return nullptr;
}

ossia::render_target_spec formatSpec(ossia::texture_format fmt, bool set)
{
  ossia::render_target_spec s;
  s.format = fmt;
  s.format_set = set;
  return s;
}

ossia::render_target_spec samplerSpec(
    ossia::texture_filter filter, ossia::texture_address_mode address,
    ossia::texture_filter mip)
{
  ossia::render_target_spec s;
  s.mag_filter = filter;
  s.min_filter = filter;
  s.mipmap_mode = mip;
  s.address_u = address;
  s.address_v = address;
  s.address_w = address;
  return s;
}

struct T2B
{
  bool skipped{};
  std::string error;
  int width{}, height{};
  halp::custom_variable_texture::texture_format format{};
};

// A CSF writing an RGBA32F image -> Texture to buffer -> Buffer Info.
T2B runTextureToBuffer(
    score::gfx::GraphicsApi api, std::optional<ossia::render_target_spec> spec)
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

    const int prod = p.addCsf(corpus("l2-csf-image-rgba32f.cs"));
    if(prod < 0)
    {
      out.error = "producer build failed: " + p.error();
      return;
    }
    auto t2bOwned = procs.make<Threedim::TextureToBuffer>(ctx);
    auto* t2bNode = t2bOwned.get();
    const int t2b = p.addNode(std::move(t2bOwned));
    auto infoOwned = procs.make<Threedim::BufferInfo>(ctx);
    auto* info = static_cast<score::gfx::OutputNode*>(infoOwned.get());
    const int bi = p.addNode(std::move(infoOwned));

    if(spec)
      p.node(t2b)->process(0, *spec);
    p.wire(p.imageOut(prod, 0), firstInput(*p.node(t2b), score::gfx::Types::Image));
    p.wire(
        p.nodeBufferOut(t2b, 0), firstInput(*p.node(bi), score::gfx::Types::Buffer));

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

    REQUIRE(!t2bNode->renderedNodes.empty());
    auto* rn = dynamic_cast<oscr::GfxRenderer<Threedim::TextureToBuffer>*>(
        t2bNode->renderedNodes.begin()->second);
    REQUIRE(rn);
    const auto& t = rn->state->inputs.texture.texture;
    out.width = t.width;
    out.height = t.height;
    out.format = t.format;
  });
  return out;
}

struct GpuInput
{
  bool skipped{};
  std::string error;
  bool hasSampler{};
  QRhiSampler::Filter mag{}, min{}, mip{};
  QRhiSampler::AddressMode u{}, v{};
  bool mipmapped{};
  bool upstreamTexture{};
  QRhiSampler::Filter magAfter{};
  QRhiSampler::AddressMode uAfter{};
};

// A CSF publishing its image -> Texture Info, whose input is a
// halp::gpu_texture_input.
GpuInput runGpuTextureInput(
    score::gfx::GraphicsApi api, const ossia::render_target_spec& spec,
    std::optional<ossia::render_target_spec> later = std::nullopt)
{
  GpuInput out;
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

    const int prod = p.addCsf(corpus("l2-csf-image-rgba32f.cs"));
    if(prod < 0)
    {
      out.error = "producer build failed: " + p.error();
      return;
    }
    auto infoOwned = procs.make<Threedim::TextureInfo>(ctx);
    auto* infoNode = infoOwned.get();
    auto* info = static_cast<score::gfx::OutputNode*>(infoOwned.get());
    const int ti = p.addNode(std::move(infoOwned));
    p.node(ti)->process(0, spec);
    p.wire(p.imageOut(prod, 0), firstInput(*p.node(ti), score::gfx::Types::Image));

    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    for(int f = 0; f < 4; ++f)
    {
      p.render(1);
      info->render();
    }

    REQUIRE(!infoNode->renderedNodes.empty());
    auto* rn = dynamic_cast<oscr::GfxRenderer<Threedim::TextureInfo>*>(
        infoNode->renderedNodes.begin()->second);
    REQUIRE(rn);
    const auto& t = rn->state->inputs.texture.texture;
    auto* upstream = p.isf(prod)->renderedNodes.begin()->second;
    auto* upTex = upstream->textureForOutput(*p.imageOut(prod, 0));
    out.upstreamTexture = upTex && t.handle == upTex;
    if(auto* tex = static_cast<QRhiTexture*>(t.handle))
      out.mipmapped = tex->flags().testFlag(QRhiTexture::MipMapped);
    if(auto* s = static_cast<QRhiSampler*>(t.sampler_handle))
    {
      out.hasSampler = true;
      out.mag = s->magFilter();
      out.min = s->minFilter();
      out.mip = s->mipmapMode();
      out.u = s->addressU();
      out.v = s->addressV();
    }

    if(later)
    {
      p.node(ti)->process(0, *later);
      for(int f = 0; f < 4; ++f)
      {
        p.render(1);
        info->render();
      }
      rn = dynamic_cast<oscr::GfxRenderer<Threedim::TextureInfo>*>(
          infoNode->renderedNodes.begin()->second);
      REQUIRE(rn);
      if(auto* s = static_cast<QRhiSampler*>(rn->state->inputs.texture.texture.sampler_handle))
      {
        out.magAfter = s->magFilter();
        out.uAfter = s->addressU();
      }
    }
  });
  return out;
}

struct GppInput
{
  bool skipped{};
  std::string error;
  QSize size;
  QRhiTexture::Format format{};
  bool mipmapped{};
  QRhiSampler::Filter mag{}, mip{};
  QRhiSampler::AddressMode u{};
  QRhiSampler::Filter magAfter{};
  QRhiSampler::AddressMode uAfter{};
  ReadbackImage img;
};

GppInput runGppGraphics(
    score::gfx::GraphicsApi api, std::optional<ossia::render_target_spec> spec,
    std::optional<ossia::render_target_spec> later = std::nullopt)
{
  GppInput out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    GfxPipeline p;
    const int prod = p.addIsf(corpus("isf-solid-color.fs"));
    auto owned = std::make_unique<oscr::CustomGpuNode<SampledInput>>(
        std::weak_ptr<Execution::ExecutionCommandQueue>{}, Gfx::exec_controls{}, 1,
        doc->context());
    auto* gpp = owned.get();
    if(spec)
      setRenderTargetSpec(*gpp, 0, *spec);
    const int idx = p.addNode(std::move(owned));
    if(prod < 0 || idx < 0)
    {
      out.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.imageOut(prod, 0), gpp->input[0]);
    const int sink = p.addSink({16, 16});
    p.wire(gpp->output[0], p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    out.img = p.readback(sink);

    auto state = [&]() -> oscr::CustomGpuRenderer<SampledInput>* {
      if(gpp->renderedNodes.empty())
        return nullptr;
      return dynamic_cast<oscr::CustomGpuRenderer<SampledInput>*>(
          gpp->renderedNodes.begin()->second);
    };
    auto* rn = state();
    REQUIRE(rn);
    auto rt = rn->m_rts.find(gpp->input[0]);
    REQUIRE(rt != rn->m_rts.end());
    REQUIRE(rt->second.texture);
    out.size = rt->second.texture->pixelSize();
    out.format = rt->second.texture->format();
    out.mipmapped = rt->second.texture->flags().testFlag(QRhiTexture::MipMapped);
    auto smp = rn->createdSamplers.find(1);
    REQUIRE(smp != rn->createdSamplers.end());
    out.mag = smp->second->magFilter();
    out.mip = smp->second->mipmapMode();
    out.u = smp->second->addressU();

    if(later)
    {
      setRenderTargetSpec(*gpp, 0, *later);
      p.render(3);
      rn = state();
      REQUIRE(rn);
      auto smp2 = rn->createdSamplers.find(1);
      REQUIRE(smp2 != rn->createdSamplers.end());
      out.magAfter = smp2->second->magFilter();
      out.uAfter = smp2->second->addressU();
    }
  });
  return out;
}

struct ComputeInput
{
  bool skipped{};
  std::string error;
  QSize size;
  QSize sizeA;
  QSize renderSize;
};

ComputeInput runGppCompute(score::gfx::GraphicsApi api, QSize size)
{
  ComputeInput out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    GfxPipeline p;
    const int prodA = p.addIsf(corpus("isf-solid-color.fs"));
    auto owned = std::make_unique<oscr::GpuComputeNode<TwoImages>>(
        std::weak_ptr<Execution::ExecutionCommandQueue>{}, Gfx::exec_controls{}, 1,
        doc->context());
    auto* node = owned.get();
    ossia::render_target_spec spec;
    spec.size = ossia::texture_size{size.width(), size.height()};
    setRenderTargetSpec(*node, 0, ossia::render_target_spec{});
    setRenderTargetSpec(*node, 1, spec);
    const int idx = p.addNode(std::move(owned));
    if(prodA < 0 || idx < 0)
    {
      out.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.imageOut(prodA, 0), node->input[0]);
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    auto st = node->renderState();
    if(!st || !st->rhi || !st->rhi->isFeatureSupported(QRhi::Compute))
    {
      out.skipped = true;
      return;
    }
    for(int i = 0; i < 3; i++)
    {
      p.render(1);
      node->render();
    }
    REQUIRE(!node->renderedNodes.empty());
    auto* rn = dynamic_cast<oscr::GpuComputeRenderer<TwoImages>*>(
        node->renderedNodes.begin()->second);
    REQUIRE(rn);
    auto rtA = rn->m_rts.find(node->input[0]);
    auto rtB = rn->m_rts.find(node->input[1]);
    REQUIRE(rtA != rn->m_rts.end());
    REQUIRE(rtB != rn->m_rts.end());
    REQUIRE(rtA->second.texture);
    REQUIRE(rtB->second.texture);
    out.sizeA = rtA->second.texture->pixelSize();
    out.size = rtB->second.texture->pixelSize();
    out.renderSize = st->renderSize;
  });
  return out;
}

template <typename T>
T* findField(Inspector::Layout& lay, const QString& label)
{
  for(int r = 0; r < lay.rowCount(); r++)
  {
    auto* l = lay.itemAt(r, QFormLayout::LabelRole);
    auto* f = lay.itemAt(r, QFormLayout::FieldRole);
    if(!l || !f || !l->widget() || !f->widget())
      continue;
    if(auto* ql = qobject_cast<QLabel*>(l->widget()); ql && ql->text() == label)
    {
      if(auto* w = qobject_cast<T*>(f->widget()))
        return w;
      if(auto* w = f->widget()->findChild<T*>())
        return w;
    }
  }
  return nullptr;
}
}

TEST_CASE(
    "An RGBA8 format set on the inlet is not the upstream's format",
    "[gfx][avnd][texture][inlet-settings][l2]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const T2B set = runTextureToBuffer(api, formatSpec(ossia::texture_format::RGBA8, true));
  if(set.skipped)
    SKIP("backend unavailable");
  INFO("error=" << set.error);
  REQUIRE(set.error.empty());
  CHECK(set.format == halp::custom_variable_texture::RGBA8);

  const T2B unset
      = runTextureToBuffer(api, formatSpec(ossia::texture_format::RGBA8, false));
  INFO("error=" << unset.error);
  REQUIRE(unset.error.empty());
  CHECK(unset.format == halp::custom_variable_texture::RGBA32F);

  const T2B none = runTextureToBuffer(api, std::nullopt);
  INFO("error=" << none.error);
  REQUIRE(none.error.empty());
  CHECK(none.format == halp::custom_variable_texture::RGBA32F);
}

TEST_CASE(
    "A GPU texture input publishes the inlet's sampler and mipmaps",
    "[gfx][avnd][texture][inlet-settings][l2]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const GpuInput nearest = runGpuTextureInput(
      api,
      samplerSpec(
          ossia::texture_filter::NEAREST, ossia::texture_address_mode::MIRROR,
          ossia::texture_filter::NONE),
      samplerSpec(
          ossia::texture_filter::LINEAR, ossia::texture_address_mode::CLAMP_TO_EDGE,
          ossia::texture_filter::NONE));
  if(nearest.skipped)
    SKIP("backend unavailable");
  INFO("error=" << nearest.error);
  REQUIRE(nearest.error.empty());
  REQUIRE(nearest.hasSampler);
  CHECK(nearest.mag == QRhiSampler::Nearest);
  CHECK(nearest.min == QRhiSampler::Nearest);
  CHECK(nearest.mip == QRhiSampler::None);
  CHECK(nearest.u == QRhiSampler::Mirror);
  CHECK(nearest.v == QRhiSampler::Mirror);
  CHECK(!nearest.mipmapped);
  CHECK(nearest.upstreamTexture);
  CHECK(nearest.magAfter == QRhiSampler::Linear);
  CHECK(nearest.uAfter == QRhiSampler::ClampToEdge);

  const GpuInput mips = runGpuTextureInput(
      api, samplerSpec(
               ossia::texture_filter::LINEAR, ossia::texture_address_mode::REPEAT,
               ossia::texture_filter::LINEAR));
  INFO("error=" << mips.error);
  REQUIRE(mips.error.empty());
  REQUIRE(mips.hasSampler);
  CHECK(mips.mag == QRhiSampler::Linear);
  CHECK(mips.mip == QRhiSampler::Linear);
  CHECK(mips.u == QRhiSampler::Repeat);
  CHECK(mips.mipmapped);
  CHECK(!mips.upstreamTexture);
}

TEST_CASE(
    "A gpp shader node builds its input from the inlet settings",
    "[gfx][avnd][gpp][texture][inlet-settings][l2]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  ossia::render_target_spec spec = samplerSpec(
      ossia::texture_filter::NEAREST, ossia::texture_address_mode::MIRROR,
      ossia::texture_filter::LINEAR);
  spec.size = ossia::texture_size{24, 16};
  spec.format = ossia::texture_format::RGBA16F;
  spec.format_set = true;
  ossia::render_target_spec later = spec;
  later.mag_filter = ossia::texture_filter::LINEAR;
  later.min_filter = ossia::texture_filter::LINEAR;
  later.address_u = ossia::texture_address_mode::REPEAT;
  later.address_v = ossia::texture_address_mode::REPEAT;

  const GppInput r = runGppGraphics(api, spec, later);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  CHECK(r.size == QSize(24, 16));
  CHECK(r.format == QRhiTexture::RGBA16F);
  CHECK(r.mipmapped);
  CHECK(r.mag == QRhiSampler::Nearest);
  CHECK(r.mip == QRhiSampler::Linear);
  CHECK(r.u == QRhiSampler::Mirror);
  CHECK(r.magAfter == QRhiSampler::Linear);
  CHECK(r.uAfter == QRhiSampler::Repeat);
  REQUIRE(r.img.valid());
  const auto c = r.img.center();
  INFO("center " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]) << "," << int(c[3]));
  CHECK(c[0] > 200);
  CHECK(c[1] < 40);
  CHECK(c[2] > 200);

  const GppInput def = runGppGraphics(api, std::nullopt);
  INFO("error=" << def.error);
  REQUIRE(def.error.empty());
  CHECK(def.format == QRhiTexture::RGBA8);
  CHECK(!def.mipmapped);
  CHECK(def.mag == QRhiSampler::Linear);
}

TEST_CASE(
    "A gpp compute node's image input takes the inlet's size",
    "[gfx][avnd][gpp][compute][inlet-settings][l2]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const ComputeInput r = runGppCompute(api, QSize{48, 32});
  if(r.skipped)
    SKIP("backend unavailable or without compute");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  CHECK(r.size == QSize(48, 32));
  CHECK(r.sizeA == r.renderSize);
  CHECK(r.renderSize != QSize(48, 32));
}

TEST_CASE(
    "Texture inlet settings are saved, loaded and executed",
    "[gfx][texture][inlet-settings][serialization][l2]")
{
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto& factories = app.interfaces<Process::PortFactoryList>();
    QObject owner;

    auto json = [&](const Gfx::TextureInlet& port, auto&& edit) {
      auto reader = score::marshall<JSONObject>(static_cast<const Process::Port&>(port));
      const QByteArray bytes = reader.toByteArray();
      rapidjson::Document doc = readJson(bytes);
      REQUIRE(!doc.HasParseError());
      edit(doc);
      JSONObject::Deserializer des{doc};
      return dynamic_cast<Gfx::TextureInlet*>(deserialize_interface(factories, des, &owner));
    };
    auto stream = [&](const Gfx::TextureInlet& port) {
      const QByteArray bytes
          = score::marshall<DataStream>(static_cast<const Process::Port&>(port));
      return dynamic_cast<Gfx::TextureInlet*>(
          deserialize_interface(factories, DataStream::Deserializer{bytes}, &owner));
    };
    auto keep = [](rapidjson::Document&) {};

    auto* rgba8 = new Gfx::TextureInlet{"in", Id<Process::Port>{1}, &owner};
    rgba8->setTextureFormat(ossia::texture_format::RGBA8);
    rgba8->setTextureMipmapMode(ossia::texture_filter::LINEAR);
    rgba8->setTextureFilter(ossia::texture_filter::NEAREST);
    rgba8->setRenderSize(QSize{24, 16});

    for(auto* loaded : {json(*rgba8, keep), stream(*rgba8)})
    {
      REQUIRE(loaded);
      CHECK(loaded->textureFormat() == std::optional{ossia::texture_format::RGBA8});
      CHECK(loaded->textureMipmapMode() == ossia::texture_filter::LINEAR);
      CHECK(loaded->textureFilter() == ossia::texture_filter::NEAREST);
      CHECK(loaded->renderSize() == std::optional{QSize{24, 16}});
    }

    auto* unset = new Gfx::TextureInlet{"in", Id<Process::Port>{2}, &owner};
    CHECK(!unset->textureFormat());
    CHECK(unset->textureMipmapMode() == ossia::texture_filter::NONE);
    for(auto* loaded : {json(*unset, keep), stream(*unset)})
    {
      REQUIRE(loaded);
      CHECK(!loaded->textureFormat());
      CHECK(loaded->textureMipmapMode() == ossia::texture_filter::NONE);
    }

    auto legacy = [](ossia::texture_format fmt) {
      return [fmt](rapidjson::Document& doc) {
        doc.RemoveMember("FormatSet");
        doc.RemoveMember("MipmapMode");
        doc["Format"].SetInt((int)fmt);
      };
    };
    auto* old8 = json(*rgba8, legacy(ossia::texture_format::RGBA8));
    REQUIRE(old8);
    CHECK(!old8->textureFormat());
    CHECK(old8->textureMipmapMode() == ossia::texture_filter::NONE);
    auto* old32 = json(*unset, legacy(ossia::texture_format::RGBA32F));
    REQUIRE(old32);
    CHECK(old32->textureFormat() == std::optional{ossia::texture_format::RGBA32F});

    ossia::texture_inlet exec;
    QObject execContext;
    rgba8->setupExecution(exec, &execContext);
    CHECK(exec.data.format == ossia::texture_format::RGBA8);
    CHECK(exec.data.format_set);
    CHECK(ossia::texture_filter(exec.data.mipmap_mode) == ossia::texture_filter::LINEAR);
    rgba8->setTextureFormat(std::nullopt);
    CHECK(!exec.data.format_set);
    rgba8->setTextureMipmapMode(ossia::texture_filter::NEAREST);
    CHECK(ossia::texture_filter(exec.data.mipmap_mode) == ossia::texture_filter::NEAREST);
    rgba8->setTextureFormat(ossia::texture_format::R32F);
    CHECK(exec.data.format == ossia::texture_format::R32F);
    CHECK(exec.data.format_set);
  });
}

TEST_CASE(
    "The texture inlet inspector shows automatic settings and the mipmap mode",
    "[gfx][texture][inlet-settings][inspector][l2]")
{
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    REQUIRE(doc);
    const auto& ctx = doc->context();
    auto* proc = add_process(
        *doc, QStringLiteral("5bd9c8e2-7f1a-4e3b-9c0d-2a4b6f8e1d72"), QString{});
    REQUIRE(proc);
    Gfx::TextureInlet* inlet{};
    for(auto* in : proc->inlets())
      if(auto* t = qobject_cast<Gfx::TextureInlet*>(in))
        inlet = t;
    REQUIRE(inlet);

    QWidget parent;
    auto* lay = new Inspector::Layout{&parent};
    Gfx::TextureInletFactory{}.setupInletInspector(*inlet, ctx, &parent, *lay, &parent);

    auto* format = findField<QComboBox>(*lay, "Format");
    REQUIRE(format);
    CHECK(format->currentText() == "Auto");
    format->setCurrentIndex(format->findText("RGBA8"));
    CHECK(inlet->textureFormat() == std::optional{ossia::texture_format::RGBA8});
    format->setCurrentIndex(format->findText("Auto"));
    CHECK(!inlet->textureFormat());

    auto* mips = findField<QComboBox>(*lay, "Mipmaps");
    REQUIRE(mips);
    CHECK(mips->currentText() == "None");
    mips->setCurrentIndex(mips->findText("Linear"));
    CHECK(inlet->textureMipmapMode() == ossia::texture_filter::LINEAR);

    auto* size = findField<QSpinBox>(*lay, "Size");
    REQUIRE(size);
    CHECK(size->text() == "Auto");
    CHECK(!size->isEnabled());
    inlet->setRenderSize(QSize{24, 16});
    CHECK(size->isEnabled());
    CHECK(size->value() == 24);
    inlet->setRenderSize(std::nullopt);
    CHECK(size->text() == "Auto");
  });
}
