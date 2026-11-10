// VST 2 plug-ins added to a document while it plays, in every way a user can
// (score_test/LivePlugin.hpp): the fixture instrument, with its state as
// parameters and as a chunk.

#include <score_test/App.hpp>
#include <score_test/LivePlugin.hpp>

#include <Vst/ApplicationPlugin.hpp>
#include <Vst/EffectModel.hpp>
#include <Vst/vst-compat.hpp>

#include <QFileInfo>

#include <catch2/catch_test_macros.hpp>

#include <dlfcn.h>

#include <optional>
#include <string>
#include <vector>

namespace
{
//! What a scan of the library at `path` lists, as its entry point says
std::optional<vst::VSTInfo> scan(const QString& path)
{
  void* h = dlopen(path.toUtf8().constData(), RTLD_NOW | RTLD_LOCAL);
  if(!h)
    return {};
  using entry_t = AEffect* (*)(audioMasterCallback);
  auto entry = reinterpret_cast<entry_t>(dlsym(h, "VSTPluginMain"));
  if(!entry)
    return {};
  AEffect* fx
      = entry([](AEffect*, int32_t op, int32_t, intptr_t, void*, float) -> intptr_t {
    switch(op)
    {
      case audioMasterVersion:
        return 2400;
      case audioMasterGetSampleRate:
        return 44100;
      case audioMasterGetBlockSize:
        return 512;
      default:
        return 0;
    }
  });
  if(!fx || fx->magic != kEffectMagic)
    return {};
  vst::VSTInfo info;
  info.path = path;
  info.prettyName = QFileInfo(path).completeBaseName();
  info.uniqueID = fx->uniqueID;
  info.controls = fx->numParams;
  info.isSynth = fx->flags & effFlagsIsSynth;
  info.isValid = true;
  fx->dispatcher(fx, effClose, 0, 0, nullptr, 0.f);
  return info;
}

void all_phases_hold(const score::GUIApplicationContext& ctx, const QString& path)
{
  auto info = scan(path);
  REQUIRE(info);
  auto& app = ctx.applicationPlugin<vst::ApplicationPlugin>();
  app.vst_infos = {*info};
  app.vst_modules.erase(info->uniqueID);
  app.vst_modules.insert({info->uniqueID, nullptr});

  score::test::live::options opt;
  opt.what = {
      Metadata<ConcreteKey_k, vst::Model>::get(), info->prettyName,
      QString::number(info->uniqueID)};

  std::vector<std::string> failures;
  int ok = 0;
  score::test::live::recorder rec;
  rec.result = [&](std::string_view phase, std::string_view verdict, std::string_view detail) {
    if(verdict == "fail")
      failures.push_back(std::string{phase} + ": " + std::string{detail});
    else if(verdict == "ok")
      ok++;
  };
  score::test::live::run(ctx, opt, rec);
  for(auto& f : failures)
    UNSCOPED_INFO(f);
  CHECK(failures.empty());
  CHECK(ok > 0);
}
}

TEST_CASE(
    "a VST 2 plug-in added, edited and removed while playing, in every way",
    "[integration][execution][vst]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    all_phases_hold(ctx, QStringLiteral(SCORE_VST_UNFLAGGED_INSTRUMENT));
  });
}

TEST_CASE(
    "a VST 2 plug-in whose state is a chunk, added, edited and removed while playing",
    "[integration][execution][vst]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    all_phases_hold(ctx, QStringLiteral(SCORE_VST_CHUNK_INSTRUMENT));
  });
}
