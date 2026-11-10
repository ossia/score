// A VST3 plug-in added, edited and removed while the document plays, in every
// way score_test/LivePlugin.hpp knows: the gain of vst3-fixtures/EditedGain,
// made with the SDK's AudioEffect and EditController as most plug-ins are.

#include <Vst3/ApplicationPlugin.hpp>
#include <Vst3/EffectModel.hpp>

#include <score_test/App.hpp>
#include <score_test/LivePlugin.hpp>

#include <catch2/catch_test_macros.hpp>

#include <pluginterfaces/vst/ivstaudioprocessor.h>

#include <string>
#include <vector>

namespace
{
//! The fixture as the scan lists it; the UID of its processor class
QString setup_fixture(vst3::ApplicationPlugin& plug)
{
  auto mdl = plug.getModule(SCORE_VST3_EDITED_GAIN);
  REQUIRE(mdl);
  vst3::AvailablePlugin p;
  p.path = QStringLiteral(SCORE_VST3_EDITED_GAIN);
  p.name = QString::fromStdString(mdl->getName());
  QString uid;
  for(const auto& cls : mdl->getFactory().classInfos())
    if(cls.category() == kVstAudioEffectClass)
    {
      p.classInfo.push_back(cls);
      uid = QString::fromStdString(cls.ID().toString());
    }
  p.isValid = true;
  plug.vst_infos = {p};
  return uid;
}
}

TEST_CASE(
    "a VST3 plug-in added, edited and removed while playing, in every way",
    "[integration][execution][vst3]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = ctx.applicationPlugin<vst3::ApplicationPlugin>();
    const QString uid = setup_fixture(plug);
    REQUIRE(!uid.isEmpty());

    score::test::live::options opt;
    opt.what = {Metadata<ConcreteKey_k, vst3::Model>::get(), {}, uid};

    std::vector<std::string> failures;
    int ok = 0;
    score::test::live::recorder rec;
    rec.result = [&](std::string_view phase, std::string_view verdict,
                     std::string_view detail) {
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
  });
}
