// A parameter a VST 2 plug-in's editor changes (audioMasterAutomate) moves
// the control of the process that has it, whether or not the process creates
// controls for the parameters that have none.

#include <Process/Dataflow/Port.hpp>

#include <Vst/ApplicationPlugin.hpp>
#include <Vst/Control.hpp>
#include <Vst/EffectModel.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QCoreApplication>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

namespace
{
vst::Model* new_process(const score::GUIApplicationContext& ctx, QObject& parent)
{
  auto& app = ctx.applicationPlugin<vst::ApplicationPlugin>();
  vst::VSTInfo info;
  info.path = QString::fromUtf8(SCORE_VST_UNFLAGGED_INSTRUMENT);
  info.prettyName = "UnflaggedInstrument";
  info.uniqueID = "sUnI"_4c;
  info.controls = 1;
  info.isValid = true;
  app.vst_infos = {info};
  app.vst_modules.insert({info.uniqueID, nullptr});
  return new vst::Model{
      TimeVal::fromMsecs(1000), QString::number(info.uniqueID),
      Id<Process::ProcessModel>{1}, &parent};
}

vst::ControlInlet* control_of(vst::Model& model, int param)
{
  for(auto* p : model.inlets())
    if(auto c = qobject_cast<vst::ControlInlet*>(p); c && c->fxNum == param)
      return c;
  return nullptr;
}
}

TEST_CASE("VST 2: a parameter changed in the editor moves its control", "[vst]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QObject parent;
    auto* model = new_process(ctx, parent);
    REQUIRE(model->fx);
    auto* control = control_of(*model, 0);
    REQUIRE(control);
    REQUIRE_FALSE(model->flags() & Process::CreateControls);

    // What the editor of the plug-in calls as its knob turns
    vst::vst_host_callback(model->fx->fx, audioMasterAutomate, 0, 0, nullptr, 0.2f);
    QCoreApplication::processEvents();
    CHECK(ossia::convert<float>(control->value()) == Catch::Approx(0.2f));

    model->setCreatingControls(true);
    vst::vst_host_callback(model->fx->fx, audioMasterAutomate, 0, 0, nullptr, 0.4f);
    QCoreApplication::processEvents();
    CHECK(ossia::convert<float>(control->value()) == Catch::Approx(0.4f));

    delete model;
    QCoreApplication::processEvents();
  });
}
