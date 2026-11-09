// A VST 2 plug-in that takes MIDI gets a MIDI input, whether or not it sets
// effFlagsIsSynth, as JuceOPL, an instrument that does not.
// A process saved without one loads with the ports it was saved with.
//
// One that can send MIDI events gets a MIDI output, which carries what it
// sends while it processes, dated in the tick.

#include <Process/Dataflow/Port.hpp>

#include <Vst/ApplicationPlugin.hpp>
#include <Vst/Control.hpp>
#include <Vst/EffectModel.hpp>
#include <Vst/Node.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QCoreApplication>

#include <libremidi/message.hpp>
#include <libremidi/ump.hpp>

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

TEST_CASE(
    "VST 2: the MIDI events a plug-in sends while it processes reach its MIDI output",
    "[vst][midi]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    register_fixture(ctx.applicationPlugin<vst::ApplicationPlugin>());

    QObject parent;
    auto* model = new vst::Model{
        TimeVal::fromMsecs(1000), QString::number(fixture_id),
        Id<Process::ProcessModel>{1}, &parent};
    REQUIRE(model->fx);
    REQUIRE(model->outlets().size() == 2);
    CHECK(model->outlets()[1]->type() == Process::PortType::Midi);
    REQUIRE(model->sendsMidi());

    constexpr int block = 256;
    ossia::execution_state state;
    state.sampleRate = 48000;
    state.bufferSize = 1000;
    {
      // As the executor makes it
      auto node = std::make_shared<vst::vst_node<false, true>>(
          model->fx, 48000, block, model->sendsMidi());
      REQUIRE(node->root_outputs().size() == 2);
      auto& in = static_cast<ossia::midi_inlet*>(node->root_inputs()[1])->data.messages;
      auto u = libremidi::ump_from_midi1(libremidi::channel_events::note_on(1, 64, 100));
      u.timestamp = 300;
      in.push_back(u);

      ossia::token_request tk;
      tk.prev_date = ossia::time_value{0};
      tk.date = ossia::time_value{1000};
      tk.start_sample = 0;
      tk.length_sample = 1000;
      tk.tempo = 120.;
      node->run(tk, ossia::exec_state_facade{&state});

      // Sent from the block that starts at 256, one frame after the note
      auto& out = node->root_outputs()[1]->cast<ossia::midi_port>().messages;
      REQUIRE(out.size() == 1);
      CHECK(out[0].timestamp == 301);
      CHECK(cmidi2_ump_get_midi2_note_note((const cmidi2_ump*)out[0].data) == 64);

      // Outside a process call, nothing is dated: nothing is kept
      out.clear();
      node->all_notes_off();
      CHECK(out.empty());
    }

    delete model;
    QCoreApplication::processEvents();
  });
}
