// The Bytebeat process as it runs: its node renders the formula with the
// controls of the model, and follows the program when the script changes.
// The execution queue is drained by hand and the node is ticked directly.

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/flicks.hpp>

#include <Bytebeat/Bytebeat.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <cmath>
#include <vector>

namespace
{
const QString bytebeat_uuid = QStringLiteral("608beeb7-e5c2-40a5-bd1a-aa7aec80f864");
constexpr int buffer = 512;
constexpr int sample_rate = 48000;

struct Running
{
  Execution::DocumentPlugin& plug;
  std::shared_ptr<ossia::graph_node> node;
  int64_t frame{};

  // One buffer of the timeline from the current position, left channel
  std::vector<double> tick()
  {
    score::test::run_exec(plug);
    auto& st = *plug.context().execState;
    st.bufferSize = buffer;
    st.sampleRate = sample_rate;

    constexpr double fps = ossia::flicks_per_second<double>;
    ossia::token_request tk{};
    tk.prev_date = ossia::time_value{int64_t(double(frame) * fps / sample_rate)};
    tk.date = ossia::time_value{int64_t(double(frame + buffer) * fps / sample_rate)};
    tk.start_sample = 0;
    tk.length_sample = buffer;
    tk.speed = 1.;
    node->run(tk, ossia::exec_state_facade{&st});
    frame += buffer;

    auto& out = *node->root_outputs()[0]->target<ossia::audio_port>();
    REQUIRE(out.channels() == 2);
    CHECK(out.channel(0) == out.channel(1));
    return {out.channel(0).begin(), out.channel(0).end()};
  }
};

std::shared_ptr<ossia::graph_node> node_of(Execution::DocumentPlugin& plug, const Process::ProcessModel& proc)
{
  REQUIRE(plug.baseScenario());
  auto& procs = plug.baseScenario()->baseInterval().processes();
  auto it = procs.find(proc.id());
  REQUIRE(it != procs.end());
  REQUIRE(it->second->node);
  return it->second->node;
}

double byte(int b)
{
  return double(b & 255) / 127.5 - 1.;
}

void setControl(Process::ProcessModel& p, int i, ossia::value v)
{
  safe_cast<Process::ControlInlet*>(p.inlets()[i])->setValue(std::move(v));
}
}

TEST_CASE("Bytebeat renders its formula at its own rate", "[bytebeat][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = safe_cast<Jit::BytebeatModel*>(score::test::add_process(*doc, bytebeat_uuid, "t"));
    REQUIRE(proc);
    REQUIRE(proc->program);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    Running r{plug, node_of(plug, *proc)};

    // t counts at 8 kHz on a 48 kHz output: each value lasts 6 samples
    auto out = r.tick();
    for(int i = 0; i < buffer; i++)
      REQUIRE(out[i] == byte(i * 8000 / sample_rate));

    // t follows the position in the timeline
    out = r.tick();
    for(int i = 0; i < buffer; i++)
      REQUIRE(out[i] == byte((buffer + i) * 8000 / sample_rate));

    // The rate and the output mode apply while it runs
    setControl(*proc, 2, 24000);
    setControl(*proc, 1, int(Jit::Bytebeat::OutputMode::SignedBytebeat));
    const int64_t start = r.frame;
    out = r.tick();
    for(int i = 0; i < buffer; i++)
    {
      const int t = int(std::floor((start + i) * 24000. / sample_rate));
      REQUIRE(out[i] == byte(t + 128));
    }

    // A new script replaces the program of the running node
    REQUIRE(proc->setScript("t*2").valid);
    const int64_t start2 = r.frame;
    out = r.tick();
    for(int i = 0; i < buffer; i++)
    {
      const int t = int(std::floor((start2 + i) * 24000. / sample_rate));
      REQUIRE(out[i] == byte(t * 2 + 128));
    }

    // So does a change of language, whose program differs for the same text
    REQUIRE(proc->setScript("t/3").valid);
    r.tick();
    setControl(*proc, 0, int(Jit::Bytebeat::Language::C));
    setControl(*proc, 1, int(Jit::Bytebeat::OutputMode::Floatbeat));
    out = r.tick();
    for(double s : out)
    {
      // An integer clamped to [-1, 1]
      REQUIRE(s == std::round(s));
    }

    plug.clear();
  });
}
