// What score's CLAP host gives a plug-in, seen from the plug-in: a mono effect
// (clap-mono-gain) that counts what a host does wrong. Its gain is its third
// parameter, after a read-only and a hidden one, and score runs it as one
// instance per channel.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/data_copy.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>

#include <Clap/EffectModel.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <dlfcn.h>

namespace
{
const QString clap_uuid = QStringLiteral("10e8d26d-1d82-4bfc-a9fb-d85ffdf04e5f");

struct errors
{
  int wrong_thread;
  int gestures;
  int foreign_cookies;
  int bad_events;
  int bad_state;
  int late_calls;
  int double_init;
  int most_params;
  int activations;
  int instances;
  int active;
  int initialised;
  double last_rate;
};

template <typename F>
F* plugin_function(const char* name)
{
  void* lib = dlopen(CLAP_MONO_GAIN_PATH, RTLD_LAZY);
  REQUIRE(lib);
  auto f = reinterpret_cast<F*>(dlsym(lib, name));
  REQUIRE(f);
  return f;
}

//! What the plug-in counted since the last call
errors report()
{
  return plugin_function<errors()>("clap_mono_gain_report")();
}

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

Clap::Model* add_mono_gain(score::Document& doc)
{
  auto* proc = score::test::add_process(
      doc, clap_uuid, QStringLiteral(CLAP_MONO_GAIN_PATH ":::io.ossia.test.mono-gain"));
  REQUIRE(proc);
  auto* clap = static_cast<Clap::Model*>(proc);
  REQUIRE(clap->handle());
  REQUIRE(clap->handle()->plugin);
  return clap;
}

Process::ControlInlet& gain_control(Clap::Model& proc)
{
  for(auto* inlet : proc.inlets())
    if(auto* ctl = qobject_cast<Process::ControlInlet*>(inlet))
      return *ctl;
  FAIL("no control");
  throw;
}

double plugin_gain(Clap::Model& proc)
{
  auto& h = *proc.handle();
  double v{};
  REQUIRE(h.ext_params->get_value(h.plugin, 3, &v));
  return v;
}

//! A document playing one mono gain, driven block by block
struct playing
{
  score::Document& doc;
  Clap::Model& proc;
  Execution::DocumentPlugin& plug;
  std::shared_ptr<ossia::graph_node> node;
  int64_t frame{};

  playing(score::Document& d, Clap::Model& p)
      : doc{d}
      , proc{p}
      , plug{d.context().plugin<Execution::DocumentPlugin>()}
  {
    plug.reload(true, score::test::base_interval(doc));
    score::test::run_exec(plug);
    REQUIRE(plug.baseScenario());
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(proc.id());
    REQUIRE(it != procs.end());
    REQUIRE(it->second->node);
    node = it->second->node;
  }

  //! One block of an input at 1 on each channel through the node, as the
  //! graph runs it, with the main thread's turn after it: the output, per
  //! channel
  std::vector<double> block(int channels)
  {
    auto& st = *plug.contextData()->execState;
    const int bs = st.bufferSize;
    for(auto* o : node->root_outputs())
      o->visit(ossia::clear_data{});
    auto& in = *node->root_inputs()[0]->target<ossia::audio_port>();
    in.set_channels(channels);
    for(auto& c : in)
      c.assign(bs, 1.);

    ossia::token_request tk;
    tk.prev_date = ossia::time_value{frame * 1000};
    tk.date = ossia::time_value{(frame + bs) * 1000};
    tk.start_sample = 0;
    tk.length_sample = bs;
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    plug.runAllCommands();
    node->run(tk, ossia::exec_state_facade{&st});
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    frame += bs;

    std::vector<double> res;
    auto& out = *node->root_outputs()[0]->target<ossia::audio_port>();
    for(auto& c : out)
      res.push_back(c.empty() ? 0. : c[bs - 1]);
    for(auto* i : node->root_inputs())
      i->visit(ossia::clear_data{});
    QCoreApplication::processEvents();
    return res;
  }

  //! What the clock's stop does: the interval stops on the execution thread,
  //! and the main thread may get its turn first
  void stop()
  {
    node.reset();
    plug.baseScenario()->baseInterval().stop();
    QCoreApplication::processEvents();
    score::test::run_exec(plug);
    plug.clear();
    spin(50);
  }
};
}

TEST_CASE(
    "CLAP: a control after a read-only and a hidden parameter sets its own parameter",
    "[integration][clap][params]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);
    REQUIRE(proc.parameterInputs().size() == 1);
    REQUIRE(proc.parameterOutputs().size() == 1);

    gain_control(proc).setValue(0.25);
    CHECK(plugin_gain(proc) == Catch::Approx(0.25));
  });
}

TEST_CASE(
    "CLAP: a control set on score's interface reaches the plug-in as a value only",
    "[integration][clap][params]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);
    report();

    auto& ctl = gain_control(proc);
    ctl.setValue(0.25);
    ctl.setValue(0.3);
    // Whatever the host sends some time after a change is sent by then
    spin(400);

    CHECK(plugin_gain(proc) == Catch::Approx(0.3));
    // clap/ext/params.h: gestures are the plug-in telling the host its own
    // editor is being used
    CHECK(report().gestures == 0);
  });
}
