// A parameter a VST 2 plug-in's editor changes (audioMasterAutomate) moves
// the control of the process that has it, whether or not the process creates
// controls for the parameters that have none. The control takes the value the
// plug-in gives with getParameter, whatever audioMasterAutomate carries, and
// that value is not sent back to the plug-in. The call neither allocates nor
// locks, so that a plug-in can make it from its audio thread.

#include "AllocCounter.hpp"

#include <Process/Dataflow/Port.hpp>

#include <Vst/ApplicationPlugin.hpp>
#include <Vst/Control.hpp>
#include <Vst/EffectModel.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

#include <thread>

namespace
{
vst::Model* new_process(
    const score::GUIApplicationContext& ctx, QObject& parent, const char* path,
    int32_t id)
{
  auto& app = ctx.applicationPlugin<vst::ApplicationPlugin>();
  vst::VSTInfo info;
  info.path = QString::fromUtf8(path);
  info.prettyName = "Fixture";
  info.uniqueID = id;
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

//! Lets the main thread run for a while: long enough for the timers of the
//! VST 2 application plug-in to fire several times
void run_main_thread(int ms = 150)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

float value_of(vst::ControlInlet& c)
{
  return ossia::convert<float>(c.value());
}

// What the editor of EditedEffect does, as effVendorSpecific
enum editor : int32_t
{
  move_in_percent = 1,
  move_normalized = 2,
  set_parameter_calls = 3,
};

intptr_t editor_moves(vst::Model& model, editor how, float normalized)
{
  return model.fx->dispatch(effVendorSpecific, how, 0, nullptr, normalized);
}

intptr_t set_parameter_calls_of(vst::Model& model)
{
  return model.fx->dispatch(effVendorSpecific, set_parameter_calls);
}
}

TEST_CASE("VST 2: a parameter changed in the editor moves its control", "[vst]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QObject parent;
    auto* model
        = new_process(ctx, parent, SCORE_VST_UNFLAGGED_INSTRUMENT, "sUnI"_4c);
    REQUIRE(model->fx);
    auto* control = control_of(*model, 0);
    REQUIRE(control);
    REQUIRE_FALSE(model->flags() & Process::CreateControls);

    // What the editor of the plug-in does as its knob turns
    model->fx->setParameter(0, 0.2f);
    vst::vst_host_callback(model->fx->fx, audioMasterAutomate, 0, 0, nullptr, 0.2f);
    run_main_thread();
    CHECK(value_of(*control) == Catch::Approx(0.2f));

    model->setCreatingControls(true);
    model->fx->setParameter(0, 0.4f);
    vst::vst_host_callback(model->fx->fx, audioMasterAutomate, 0, 0, nullptr, 0.4f);
    run_main_thread();
    CHECK(value_of(*control) == Catch::Approx(0.4f));

    delete model;
    QCoreApplication::processEvents();
  });
}

TEST_CASE(
    "VST 2: a control takes the plug-in's value, not what audioMasterAutomate "
    "carries",
    "[vst]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QObject parent;
    auto* model = new_process(ctx, parent, SCORE_VST_EDITED_EFFECT, "sEdE"_4c);
    REQUIRE(model->fx);
    auto* control = control_of(*model, 0);
    REQUIRE(control);
    run_main_thread();

    // Dragonfly Plate's Dry Level slider, dragged to 61 %, sends 61
    for(float v : {0.613f, 0.3f, 0.95f, 0.f})
    {
      editor_moves(*model, move_in_percent, v);
      run_main_thread();
      CHECK(value_of(*control) == Catch::Approx(v));
      CHECK(model->fx->getParameter(0) == Catch::Approx(v));
    }
    CHECK(set_parameter_calls_of(*model) == 0);

    delete model;
    QCoreApplication::processEvents();
  });
}

TEST_CASE("VST 2: a value the editor sets is not sent back to the plug-in", "[vst]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QObject parent;
    auto* model = new_process(ctx, parent, SCORE_VST_EDITED_EFFECT, "sEdE"_4c);
    REQUIRE(model->fx);
    auto* control = control_of(*model, 0);
    REQUIRE(control);
    run_main_thread();

    // A drag moves the parameter several times before the main thread runs:
    // the knob stays where the drag left it
    editor_moves(*model, move_normalized, 0.3f);
    editor_moves(*model, move_normalized, 0.4f);
    editor_moves(*model, move_normalized, 0.5f);
    run_main_thread();
    CHECK(value_of(*control) == Catch::Approx(0.5f));
    CHECK(model->fx->getParameter(0) == Catch::Approx(0.5f));
    CHECK(set_parameter_calls_of(*model) == 0);

    // A value set in score still reaches the plug-in
    control->setValue(0.25f);
    CHECK(model->fx->getParameter(0) == Catch::Approx(0.25f));
    CHECK(set_parameter_calls_of(*model) == 1);

    delete model;
    QCoreApplication::processEvents();
  });
}

TEST_CASE(
    "VST 2: audioMasterAutomate from the audio thread does not allocate", "[vst]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QObject parent;
    auto* model = new_process(ctx, parent, SCORE_VST_EDITED_EFFECT, "sEdE"_4c);
    REQUIRE(model->fx);
    auto* control = control_of(*model, 0);
    REQUIRE(control);
    run_main_thread();

    int allocations = -1;
    std::thread audio{[&] {
      alloc_counter::scope s;
      editor_moves(*model, move_normalized, 0.7f);
      editor_moves(*model, move_in_percent, 0.6f);
      allocations = s.allocations();
    }};
    audio.join();
    CHECK(allocations == 0);

    run_main_thread();
    CHECK(value_of(*control) == Catch::Approx(0.6f));
    CHECK(set_parameter_calls_of(*model) == 0);

    delete model;
    QCoreApplication::processEvents();
  });
}

TEST_CASE(
    "VST 2: a plug-in still processing once its process is deleted calls the "
    "host safely",
    "[vst]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QObject parent;
    auto* model = new_process(ctx, parent, SCORE_VST_EDITED_EFFECT, "sEdE"_4c);
    REQUIRE(model->fx);
    run_main_thread();

    // What the node holds: the process is deleted on the main thread while
    // the audio thread is still in the tick that runs the node
    std::shared_ptr<vst::AEffectWrapper> node_ref = model->fx;
    const auto* time = &node_ref->info;
    AEffect* effect = node_ref->fx;
    effect->dispatcher(effect, effVendorSpecific, 6, 0, nullptr, 0.f);
    delete model;

    int allocations = -1;
    std::thread audio{[&] {
      float in[64]{}, out[64]{};
      float* ins[1]{in};
      float* outs[1]{out};
      alloc_counter::scope s;
      effect->processReplacing(effect, ins, outs, 64);
      allocations = s.allocations();
    }};
    audio.join();
    CHECK(allocations == 0);
    CHECK(
        effect->dispatcher(effect, effVendorSpecific, 7, 0, nullptr, 0.f)
        == reinterpret_cast<intptr_t>(time));

    // The node is gone too; the plug-in is closed later, on the main thread,
    // and one of its own threads may call the host until then
    node_ref.reset();
    std::thread own{[&] {
      float in[64]{}, out[64]{};
      float* ins[1]{in};
      float* outs[1]{out};
      effect->processReplacing(effect, ins, outs, 64);
    }};
    own.join();
    CHECK(
        effect->dispatcher(effect, effVendorSpecific, 7, 0, nullptr, 0.f)
        == reinterpret_cast<intptr_t>(time));
    run_main_thread();
  });
}
