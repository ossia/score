// A VST 2 plug-in that takes MIDI gets a MIDI input, whether or not it sets
// effFlagsIsSynth, as JuceOPL, an instrument that does not.
// A process saved without one loads with the ports it was saved with.

#include <Process/Dataflow/Port.hpp>

#include <Vst/ApplicationPlugin.hpp>
#include <Vst/Control.hpp>
#include <Vst/EffectModel.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QCoreApplication>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

namespace
{
constexpr int32_t fixture_id = "sUnI"_4c;

void register_fixture(vst::ApplicationPlugin& app)
{
  vst::VSTInfo info;
  info.path = QStringLiteral(SCORE_VST_UNFLAGGED_INSTRUMENT);
  info.prettyName = "UnflaggedInstrument";
  info.uniqueID = fixture_id;
  info.controls = 1;
  info.isValid = true;
  app.vst_infos = {info};
  app.vst_modules.insert({fixture_id, nullptr});
}

vst::ControlInlet* control_of(const vst::Model& m)
{
  for(auto* p : m.inlets())
    if(auto c = qobject_cast<vst::ControlInlet*>(p))
      return c;
  return nullptr;
}
}

TEST_CASE(
    "VST 2: a plug-in that receives MIDI events gets a MIDI input without the synth "
    "flag",
    "[vst][midi]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    register_fixture(ctx.applicationPlugin<vst::ApplicationPlugin>());

    QObject parent;
    auto* model = new vst::Model{
        TimeVal::fromMsecs(1000), QString::number(fixture_id),
        Id<Process::ProcessModel>{1}, &parent};
    REQUIRE(model->fx);
    REQUIRE(!(model->fx->fx->flags & effFlagsIsSynth));

    REQUIRE(model->inlets().size() == 3);
    CHECK(model->inlets()[0]->type() == Process::PortType::Audio);
    CHECK(model->inlets()[1]->type() == Process::PortType::Midi);
    CHECK(model->takesMidi());
    REQUIRE(control_of(*model));
    CHECK(control_of(*model)->fxNum == 0);

    const auto saved = score::marshall<JSONObject>(
        static_cast<const Process::ProcessModel&>(*model));

    SECTION("it is saved and loaded with it")
    {
      const auto doc = readJson(saved.toByteArray());
      JSONObject::Deserializer des{doc};
      auto* loaded = new vst::Model{des, &parent};
      CHECK(loaded->takesMidi());
      REQUIRE(control_of(*loaded));
      CHECK(control_of(*loaded)->fxNum == 0);
      delete loaded;
    }

    SECTION("a process saved without one loads without one")
    {
      auto doc = readJson(saved.toByteArray());
      auto& inlets = doc["Inlets"];
      REQUIRE(inlets.IsArray());
      inlets.Erase(inlets.Begin() + 1);
      JSONObject::Deserializer des{doc};
      auto* loaded = new vst::Model{des, &parent};
      CHECK(!loaded->takesMidi());
      REQUIRE(loaded->inlets().size() == 2);
      REQUIRE(control_of(*loaded));
      CHECK(control_of(*loaded)->fxNum == 0);
      delete loaded;
    }

    delete model;
    QCoreApplication::processEvents();
  });
}
