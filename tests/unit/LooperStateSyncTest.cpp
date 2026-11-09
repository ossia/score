// A state that sets a looper's Mode to Record, with Quantif on the bar, starts
// the recording on the bar line the state is on: the state's message reaches
// the looper in the buffer the state is in, as a cable would carry it.

#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <LocalTree/ScriptableProcessComponent.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/tick_setup.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_interval.hpp>

#include <QApplication>

#include <Avnd/Factories.hpp>
#include <Crousti/Executor.hpp>
#include <Fx/Looper/AudioLooper.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <cmath>
#include <cstdint>
#include <optional>

namespace
{
using AudioLooper = Nodes::AudioLooper::Node;
using LoopMode = AudioLooper::LoopMode;

const QString audio_looper_uuid = QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6");

struct sync_case
{
  double tempo;
  int num, den;
  //! The frames of each tick; never more than the engine was set up with.
  int buffer;
  //! The box holding the looper starts this many quarters after the score.
  double box_start_quarters;
  //! The state is this many quarters after the bar line.
  double state_after_quarters;
};

void process_events()
{
  for(int i = 0; i < 10; i++)
  {
    QCoreApplication::sendPostedEvents();
    QApplication::processEvents();
  }
}

double bar_quarters(const sync_case& c, int bar)
{
  return double(bar - 1) * 4. * double(c.num) / double(c.den);
}

//! A date of the score for a position in quarters: the score's dates count
//! at the root tempo, and its tempo plays them faster or slower.
TimeVal score_date(double quarters)
{
  return TimeVal{int64_t(std::llround(
      quarters * 60. / ossia::root_tempo * ossia::flicks_per_second<double>))};
}

ossia::time_value flicks(double seconds)
{
  return ossia::time_value{
      int64_t(std::llround(seconds * ossia::flicks_per_second<double>))};
}

struct recording
{
  //! The frame of the score where the looper's recording starts, if it does.
  std::optional<int64_t> start;
  double rate{};

  int64_t frame_of_bar(const sync_case& c, int bar) const
  {
    return int64_t(std::llround(bar_quarters(c, bar) * 60. / c.tempo * rate));
  }
};

recording record_from_state(const score::GUIApplicationContext& ctx, const sync_case& c)
{
  auto* doc = score::test::new_document(ctx);
  REQUIRE(doc);
  auto& scenario = score::test::base_scenario(*doc);

  // The looper, in a box that plays from the start of the score...
  Process::ProcessModel* proc{};
  Scenario::IntervalModel* box{};
  {
    Scenario::Command::Macro m{
        new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
    auto& start = scenario.state(scenario.startEvent().states().front());
    auto from = start.id();
    if(c.box_start_quarters > 0.)
    {
      auto& lead = m.createIntervalAfter(
          scenario, from, Scenario::Point{score_date(c.box_start_quarters), 0.2});
      from = lead.endState();
    }
    box = &m.createIntervalAfter(scenario, from, Scenario::Point{score_date(64.), 0.2});
    proc = m.createProcessInNewSlot(
        *box, UuidKey<Process::ProcessModel>::fromString(audio_looper_uuid), QString{});
    m.commit();
  }
  REQUIRE(proc);

  // ... published, and a state on the bar line asking it to record.
  CommandDispatcher<> disp{doc->context().commandStack};
  disp.submit<Process::SetProcessScriptable>(*proc, true);
  process_events();
  auto& mode = score::test::control_named(*proc, QStringLiteral("Mode"));
  const auto address = LocalTree::scriptableAddress(mode);
  REQUIRE(!address.device.isEmpty());

  const auto state_date = score_date(bar_quarters(c, 3) + c.state_after_quarters);
  Scenario::StateModel* state{};
  {
    Scenario::Command::Macro m{
        new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
    // A state of its own on the start event: a state starts one interval.
    auto& from = m.createState(scenario, scenario.startEvent().id(), 0.6);
    auto& lead = m.createIntervalAfter(
        scenario, from.id(), Scenario::Point{state_date, 0.6});
    state = &scenario.state(lead.endState());
    m.commit();
  }
  disp.submit<Scenario::Command::AddMessagesToState>(
      *state, State::MessageList{
                  {State::AddressAccessor{address}, ossia::value{std::string{"Record"}}}});
  process_events();

  auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
  plug.reload(true, score::test::base_interval(*doc));
  score::test::run_exec(plug);
  REQUIRE(plug.baseScenario());

  Execution::IntervalComponent* comp{};
  for(auto* cmp : box->components())
    if(auto ic = dynamic_cast<Execution::IntervalComponent*>(cmp))
      comp = ic;
  REQUIRE(comp);
  auto it = comp->processes().find(proc->id());
  REQUIRE(it != comp->processes().end());
  auto& node = static_cast<oscr::safe_node<AudioLooper>&>(*it->second->node);
  auto& looper = node.impl.effect;
  looper.inputs.quantif.value = halp::grid_value::bars(1);

  auto& ectx = plug.context();
  auto& st = *ectx.execState;
  REQUIRE(c.buffer <= st.bufferSize);
  auto& itv = *plug.baseScenario()->baseInterval().OSSIAInterval();

  recording rec;
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  {
    ossia::tempo_curve curve;
    curve.set_x0(0);
    curve.set_y0(c.tempo);
    curve.add_point(
        [](double, double a, double b) { return a + 0. * b; },
        int64_t(1e6 * ossia::flicks_per_second<double>), c.tempo);
    itv.set_tempo_curve(std::move(curve));
  }
  itv.set_time_signature_map(ossia::time_signature_map{
      {ossia::time_value{}, ossia::time_signature{uint16_t(c.num), uint16_t(c.den)}}});
  const ossia::time_value end = flicks(1e5);
  itv.set_min_duration(end);
  itv.set_nominal_duration(end);
  itv.set_max_duration(end);
  itv.start_and_tick();
  auto tick = ossia::make_tick(
      ossia::tick_setup_options{}, st, *ectx.execGraph,
      itv, plug.baseScenario()->baseScenario(), {});

  // Up to a little past the bar after the one the state is on.
  rec.rate = st.sampleRate;
  const int64_t last = rec.frame_of_bar(c, 4) + st.sampleRate / 4;
  int64_t frames = 0;
  while(frames < last && !rec.start)
  {
    tick(c.buffer, double(frames) / st.sampleRate);
    frames += c.buffer;
    if(looper.transport.actualMode == LoopMode::Record)
      rec.start = frames - looper.layers.length;
  }
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  plug.clear();
  QApplication::processEvents();
  return rec;
}
}

TEST_CASE(
    "A state on a bar line starts a bar-synced recording on that bar line",
    "[looper][exec][state]")
{
  // The buffers put the bar line anywhere in a tick, its first frame included.
  for(const sync_case c : {
          sync_case{120., 4, 4, 512, 0., 0.},
          sync_case{120., 4, 4, 300, 0., 0.},
          sync_case{120., 4, 4, 64, 0., 0.},
          sync_case{97., 3, 4, 512, 0., 0.},
          sync_case{97., 3, 4, 77, 0., 0.},
          sync_case{140., 7, 8, 441, 0., 0.},
          sync_case{140., 7, 8, 128, 0., 0.},
          sync_case{120., 4, 4, 512, 0.6, 0.},
          sync_case{97., 3, 4, 300, 1.4, 0.},
      })
  {
    DYNAMIC_SECTION(
        c.tempo << " bpm, " << c.num << "/" << c.den << ", buffers of " << c.buffer
                << ", box at quarter " << c.box_start_quarters)
    {
      score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
        const auto rec = record_from_state(ctx, c);
        REQUIRE(rec.start);
        const auto bar3 = rec.frame_of_bar(c, 3);
        INFO("recording from frame " << *rec.start << ", bar 3 at frame " << bar3);
        CHECK(std::abs(*rec.start - bar3) <= 1);
      });
    }
  }
}

TEST_CASE(
    "A state just after a bar line starts a bar-synced recording on the next one",
    "[looper][exec][state]")
{
  // Further after it than a buffer: within the buffer the bar line is in, the
  // looper takes up what it is asked for on the bar line.
  for(const sync_case c : {
          sync_case{120., 4, 4, 512, 0., 0.04},
          sync_case{120., 4, 4, 64, 0., 0.004},
          sync_case{97., 3, 4, 300, 0., 0.05},
          sync_case{140., 7, 8, 128, 0.6, 0.02},
      })
  {
    DYNAMIC_SECTION(
        c.tempo << " bpm, " << c.num << "/" << c.den << ", buffers of " << c.buffer
                << ", " << c.state_after_quarters << " quarters after the bar line")
    {
      score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
        const auto rec = record_from_state(ctx, c);
        REQUIRE(rec.start);
        const auto bar4 = rec.frame_of_bar(c, 4);
        INFO("recording from frame " << *rec.start << ", bar 4 at frame " << bar4);
        CHECK(std::abs(*rec.start - bar4) <= 1);
      });
    }
  }
}
