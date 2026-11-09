// A VST 2 plug-in in a document that plays: a parameter its editor changes is
// not set back on it by the execution, and a parameter it adds once open gets
// a control when its editor moves it.

#include <score_test/App.hpp>
#include <score_test/LivePlugin.hpp>

#include <Vst/ApplicationPlugin.hpp>
#include <Vst/Control.hpp>
#include <Vst/EffectModel.hpp>
#include <Vst/vst-compat.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <chrono>
#include <thread>

namespace
{
// What EditedEffect answers to effVendorSpecific
enum vendor : int32_t
{
  move_normalized = 2,
  set_parameter_calls = 3,
  blocks = 4,
  add_parameter = 5,
};

intptr_t ask(const vst::AEffectWrapper& fx, vendor v, float opt = 0.f)
{
  return fx.dispatch(effVendorSpecific, v, 0, nullptr, opt);
}

score::test::live::options edited_effect(const score::GUIApplicationContext& ctx)
{
  auto& app = ctx.applicationPlugin<vst::ApplicationPlugin>();
  vst::VSTInfo info;
  info.path = QStringLiteral(SCORE_VST_EDITED_EFFECT);
  info.prettyName = "EditedEffect";
  info.uniqueID = "sEdE"_4c;
  info.controls = 1;
  info.isValid = true;
  app.vst_infos = {info};
  app.vst_modules.erase(info.uniqueID);
  app.vst_modules.insert({info.uniqueID, nullptr});

  score::test::live::options opt;
  opt.what = {
      Metadata<ConcreteKey_k, vst::Model>::get(), info.prettyName,
      QString::number(info.uniqueID)};
  return opt;
}

vst::ControlInlet* control_of(vst::Model& model, int param)
{
  for(auto* p : model.inlets())
    if(auto c = qobject_cast<vst::ControlInlet*>(p); c && c->fxNum == param)
      return c;
  return nullptr;
}

vst::Model& add_playing(score::test::live::session& s)
{
  auto& proc = static_cast<vst::Model&>(s.drop_process(s.root()));
  s.plays_on(200);
  s.runs(proc);
  REQUIRE(proc.fx);
  REQUIRE(ask(*proc.fx, blocks) > 0);
  return proc;
}
}

TEST_CASE(
    "VST 2: a parameter the editor changes while playing is not set back on the "
    "plug-in",
    "[integration][execution][vst]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    const auto opt = edited_effect(ctx);
    score::test::live::session s{ctx, opt};
    auto& proc = add_playing(s);
    // Or its controls, through the parameters they are published as
    const bool scriptable = GENERATE(false, true);
    CAPTURE(scriptable);
    if(scriptable)
    {
      proc.setScriptable(true);
      s.plays_on(200);
    }
    auto* control = control_of(proc, 0);
    REQUIRE(control);
    auto& fx = *proc.fx;

    const auto before = ask(fx, set_parameter_calls);
    s.plays_on(200);
    CHECK(ask(fx, set_parameter_calls) == before);

    // The editor moves the parameter: the control follows, the plug-in keeps
    // its value and is not given it again
    ask(fx, move_normalized, 0.3f);
    s.plays_on(200);
    CHECK(ossia::convert<float>(control->value()) == Catch::Approx(0.3f));
    CHECK(fx.getParameter(0) == Catch::Approx(0.3f));
    CHECK(ask(fx, set_parameter_calls) == before);

    // A drag goes on while the main thread is busy: the execution leaves the
    // plug-in where the editor took it
    ask(fx, move_normalized, 0.6f);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(fx.getParameter(0) == Catch::Approx(0.6f));
    s.plays_on(200);
    CHECK(ossia::convert<float>(control->value()) == Catch::Approx(0.6f));
    CHECK(fx.getParameter(0) == Catch::Approx(0.6f));
    CHECK(ask(fx, set_parameter_calls) == before);

    // A value set in score reaches the plug-in
    control->setValue(0.25f);
    s.plays_on(200);
    CHECK(fx.getParameter(0) == Catch::Approx(0.25f));
  });
}


TEST_CASE(
    "VST 2: a parameter the plug-in adds once open gets a control when its editor "
    "moves it",
    "[integration][execution][vst]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    const auto opt = edited_effect(ctx);
    score::test::live::session s{ctx, opt};
    auto& proc = add_playing(s);
    auto& fx = *proc.fx;
    REQUIRE(control_of(proc, 0));
    REQUIRE_FALSE(control_of(proc, 1));
    proc.setCreatingControls(true);

    ask(fx, add_parameter, 0.7f);
    s.plays_on(200);
    auto* added = control_of(proc, 1);
    REQUIRE(added);
    CHECK(ossia::convert<float>(added->value()) == Catch::Approx(0.7f));

    // From now on it has a flag of its own
    ask(fx, add_parameter, 0.2f);
    s.plays_on(200);
    CHECK(ossia::convert<float>(added->value()) == Catch::Approx(0.2f));
    CHECK(fx.getParameter(1) == Catch::Approx(0.2f));
  });
}
