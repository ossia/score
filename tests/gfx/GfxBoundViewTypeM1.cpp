// =============================================================================
// Agent M1: every texture a live pipeline samples has the view type its shader
// declares. A cable cut from an array input (a plain cable or a self-cable)
// used to rebind the input to the 2D empty texture, which Metal's validation
// reports as "incorrect type of texture (MTLTextureType2D) bound ... (expect
// MTLTextureType2DArray)" and which Vulkan rejects as a view-type mismatch.
//
// The check walks QRhi's registry of live resources: for each graphics or
// compute pipeline, the textures bound in its shader resource bindings are
// compared with the sampler types in its shaders' reflection, on every
// backend.
// =============================================================================
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/RenderList.hpp>

#include <QtGui/private/qrhi_p.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
QHash<QRhiResource*, bool>& live_resources(QRhiImplementation& rhi);

template <QHash<QRhiResource*, bool> QRhiImplementation::* Member>
struct live_resources_access
{
  friend QHash<QRhiResource*, bool>& live_resources(QRhiImplementation& rhi)
  {
    return rhi.*Member;
  }
};
template struct live_resources_access<&QRhiImplementation::resources>;

QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(file);
}

enum class View
{
  Other,
  Tex2D,
  Array,
  Volume,
  Cube
};

View expected_view(QShaderDescription::VariableType t)
{
  switch(t)
  {
    case QShaderDescription::Sampler2D:
      return View::Tex2D;
    case QShaderDescription::Sampler2DArray:
      return View::Array;
    case QShaderDescription::Sampler3D:
      return View::Volume;
    case QShaderDescription::SamplerCube:
      return View::Cube;
    default:
      return View::Other;
  }
}

View bound_view(const QRhiTexture& tex)
{
  const auto f = tex.flags();
  if(f.testFlag(QRhiTexture::CubeMap))
    return View::Cube;
  if(f.testFlag(QRhiTexture::ThreeDimensional))
    return View::Volume;
  if(f.testFlag(QRhiTexture::TextureArray))
    return View::Array;
  return View::Tex2D;
}

const char* view_name(View v)
{
  switch(v)
  {
    case View::Tex2D:
      return "2D";
    case View::Array:
      return "2D array";
    case View::Volume:
      return "3D";
    case View::Cube:
      return "cube";
    default:
      return "other";
  }
}

template <typename Pipeline>
void check_pipeline(
    const Pipeline& pip, const QHash<QRhiResource*, bool>& live,
    std::vector<std::string>& out)
{
  auto* srb = pip.shaderResourceBindings();
  if(!srb || !live.contains(srb))
    return;

  std::map<int, View> declared;
  const auto collect = [&](const QShader& shader) {
    for(const auto& v : shader.description().combinedImageSamplers())
      declared[v.binding] = expected_view(v.type);
  };
  if constexpr(std::is_same_v<Pipeline, QRhiComputePipeline>)
    collect(pip.shaderStage().shader());
  else
    for(auto it = pip.cbeginShaderStages(); it != pip.cendShaderStages(); ++it)
      collect(it->shader());

  for(auto it = srb->cbeginBindings(); it != srb->cendBindings(); ++it)
  {
    const auto* d = QRhiImplementation::shaderResourceBindingData(*it);
    if(d->type != QRhiShaderResourceBinding::SampledTexture)
      continue;
    const auto decl = declared.find(d->binding);
    if(decl == declared.end() || decl->second == View::Other)
      continue;
    for(int i = 0; i < d->u.stex.count; i++)
    {
      auto* tex = d->u.stex.texSamplers[i].tex;
      if(!tex || !live.contains(tex))
        continue;
      const View b = bound_view(*tex);
      if(b != decl->second)
        out.push_back(
            "pipeline \"" + pip.name().toStdString() + "\" binding "
            + std::to_string(d->binding) + ": shader declares a "
            + view_name(decl->second) + " sampler, bound texture \""
            + tex->name().toStdString() + "\" is " + view_name(b));
    }
  }
}

std::vector<std::string> view_mismatches(GfxPipeline& p)
{
  std::vector<std::string> out;
  std::set<QRhiImplementation*> seen;
  for(const auto& rl : p.graph().renderLists())
  {
    if(!rl || !rl->state.rhi)
      continue;
    auto* batch = rl->state.rhi->nextResourceUpdateBatch();
    if(!batch)
      continue;
    auto& impl = *QRhiResourceUpdateBatchPrivate::get(batch)->rhi;
    batch->release();
    if(!seen.insert(&impl).second)
      continue;
    const auto& live = live_resources(impl);
    for(auto it = live.cbegin(); it != live.cend(); ++it)
    {
      auto* res = it.key();
      switch(res->resourceType())
      {
        case QRhiResource::GraphicsPipeline:
          check_pipeline(static_cast<const QRhiGraphicsPipeline&>(*res), live, out);
          break;
        case QRhiResource::ComputePipeline:
          check_pipeline(static_cast<const QRhiComputePipeline&>(*res), live, out);
          break;
        default:
          break;
      }
    }
  }
  return out;
}

struct Stage
{
  std::string when;
  std::vector<std::string> mismatches;
};
}

TEST_CASE(
    "M1: an array input keeps an array texture bound when its cable is cut",
    "[gfx][feedback][m1]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool self = GENERATE(true, false);
  CAPTURE(backend_name(api), self);

  bool skipped = false;
  std::string skip_reason, error;
  std::vector<Stage> stages;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("d2selffb-array.fs"));
    const int r = p.addIsf(corpus("d2selffb-array-reader.fs"));
    const int sink = p.addSink({32, 32});
    if(a < 0 || r < 0)
    {
      error = p.error().empty() ? "build failed" : p.error();
      return;
    }
    p.wire(p.imageOut(a, 0), p.imageIn(r, 0));
    p.wire(p.imageOut(r, 0), p.sinkInput(sink));
    if(self)
      p.wire(p.imageOut(a, 0), p.imageIn(a, 0), Process::CableType::DelayedGlutton);
    if(!p.create(api))
    {
      skipped = p.skipped();
      skip_reason = p.skipReason();
      error = skipped ? std::string{} : p.error();
      return;
    }

    p.render(2);
    stages.push_back({"connected", view_mismatches(p)});
    if(self)
      p.removeEdgeIncremental(p.imageOut(a, 0), p.imageIn(a, 0));
    else
      p.removeEdgeIncremental(p.imageOut(a, 0), p.imageIn(r, 0));
    stages.push_back({"cut", view_mismatches(p)});
    p.render(2);
    stages.push_back({"rendered after the cut", view_mismatches(p)});
    if(!p.readback(sink).valid())
      error = "empty readback";
  });
  if(skipped)
    SKIP(skip_reason);
  REQUIRE(error.empty());
  REQUIRE(stages.size() == 3);
  for(const auto& s : stages)
  {
    CAPTURE(s.when);
    for(const auto& m : s.mismatches)
      UNSCOPED_INFO(m);
    CHECK(s.mismatches.empty());
  }
}
