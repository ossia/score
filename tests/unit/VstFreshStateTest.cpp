// A new VST 2 process is on the program the plug-in says it opens on, or on
// its first when it names none. Permut8 opens on another than its first, its
// first being an empty one; plug-ins made with the
// VST SDK may set themselves up only when a program is selected. Either way
// the programs are still there to choose from. A program is selected as a
// program change, between effBeginSetProgram and effEndSetProgram, as Carla,
// Audacity and OpenMPT do.

#include <Process/Dataflow/Port.hpp>

#include <Vst/ApplicationPlugin.hpp>
#include <Vst/Control.hpp>
#include <Vst/EffectModel.hpp>

#include <QCoreApplication>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

namespace
{
//! The level of a new process of the fixture in `path`, as the plug-in and as
//! its control have it, and the presets it lists
void check_new_process(
    const score::GUIApplicationContext& ctx, const char* path, int32_t id, float level)
{
  auto& app = ctx.applicationPlugin<vst::ApplicationPlugin>();
  vst::VSTInfo info;
  info.path = QString::fromUtf8(path);
  info.prettyName = "UnflaggedInstrument";
  info.uniqueID = id;
  info.controls = 1;
  info.isValid = true;
  app.vst_infos = {info};
  app.vst_modules.insert({id, nullptr});

  QObject parent;
  auto* model = new vst::Model{
      TimeVal::fromMsecs(1000), QString::number(id), Id<Process::ProcessModel>{1},
      &parent};
  REQUIRE(model->fx);

  CHECK(model->fx->getParameter(0) == level);
  vst::ControlInlet* control{};
  for(auto* p : model->inlets())
    if(auto c = qobject_cast<vst::ControlInlet*>(p))
      control = c;
  REQUIRE(control);
  CHECK(ossia::convert<float>(control->value()) == level);

  // Selected as a program change: between effBeginSetProgram and
  // effEndSetProgram
  CHECK(model->dispatch(effVendorSpecific, 1) == 1);
  CHECK(model->dispatch(effVendorSpecific, 0) == 0);

  const auto presets
      = static_cast<const Process::ProcessModel*>(model)->builtinPresets();
  CHECK(presets.size() == 2);

  // A program chosen among its presets too
  static_cast<Process::ProcessModel*>(model)->loadPreset(presets[0]);
  CHECK(model->fx->getParameter(0) == 0.75f);
  CHECK(model->dispatch(effVendorSpecific, 1) == 2);
  CHECK(model->dispatch(effVendorSpecific, 0) == 0);

  delete model;
  QCoreApplication::processEvents();
}
}

TEST_CASE("VST 2: a new process stays on the program the plug-in opens on", "[vst]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // Its second program, not its first (0.75)
    check_new_process(ctx, SCORE_VST_UNFLAGGED_INSTRUMENT, "sUnI"_4c, 0.5f);
  });
}

TEST_CASE("VST 2: a plug-in that opens on no program has one selected", "[vst]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // Its first, without which its level is 0
    check_new_process(ctx, SCORE_VST_UNPROGRAMMED_INSTRUMENT, "sUnP"_4c, 0.75f);
  });
}
