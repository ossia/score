// What score's CLAP host gives a plug-in, seen from the plug-in: a mono effect
// (clap-mono-gain) that counts what a host does wrong. Its gain is its third
// parameter, after a read-only and a hidden one, and score runs it as one
// instance per channel.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>

#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

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

//! The editor of the process's instance, the first of a mono node, sets the
//! gain
void set_from_ui(Clap::Model& proc, double v)
{
  plugin_function<void(const clap_plugin_t*, double)>("clap_mono_gain_set_from_ui")(
      proc.handle()->plugin, v);
}

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

Clap::Model* add_gain(score::Document& doc, const QString& id)
{
  auto* proc = score::test::add_process(
      doc, clap_uuid, QStringLiteral(CLAP_MONO_GAIN_PATH ":::") + id);
  REQUIRE(proc);
  auto* clap = static_cast<Clap::Model*>(proc);
  REQUIRE(clap->handle());
  REQUIRE(clap->handle()->plugin);
  return clap;
}

Clap::Model* add_mono_gain(score::Document& doc)
{
  return add_gain(doc, QStringLiteral("io.ossia.test.mono-gain"));
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

TEST_CASE(
    "CLAP: a rescan of the parameters' information keeps a plug-in with hidden ones",
    "[integration][clap][params]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);

    // A plug-in whose parameters changed is told so in a warning
    static std::atomic<int> layout_changes{};
    static QtMessageHandler previous{};
    layout_changes = 0;
    previous = qInstallMessageHandler(
        [](QtMsgType t, const QMessageLogContext& c, const QString& msg) {
      if(msg.contains(QStringLiteral("parameter layout changed")))
        layout_changes++;
      if(previous)
        previous(t, c, msg);
    });

    plugin_function<void(const clap_plugin_t*, uint32_t)>("clap_mono_gain_rescan")(
        proc.handle()->plugin, CLAP_PARAM_RESCAN_INFO);
    spin(50);
    qInstallMessageHandler(previous);
    CHECK(layout_changes == 0);
  });
}

TEST_CASE(
    "CLAP: the instances of a mono plug-in call no main-thread function from the "
    "audio thread",
    "[integration][clap][mono]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);
    playing p{*doc, proc};
    report();

    for(int i = 0; i < 4; i++)
      p.block(4);
    gain_control(proc).setValue(0.4);
    set_from_ui(proc, 0.6);
    for(int i = 0; i < 4; i++)
      p.block(4);

    CHECK(report().wrong_thread == 0);
    p.stop();
  });
}

TEST_CASE(
    "CLAP: each instance of a mono plug-in gets the events of its block, well formed",
    "[integration][clap][mono]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);
    playing p{*doc, proc};
    // The first block has the value the control starts with as well
    p.block(4);
    CHECK(report().bad_events == 0);

    // One change of the one control in each block: one value in each call
    for(int i = 0; i < 6; i++)
    {
      gain_control(proc).setValue(0.1 * (i + 1));
      p.block(4);
    }
    const auto e = report();
    CHECK(e.bad_events == 0);
    CHECK(e.most_params == 1);
    p.stop();
  });
}

TEST_CASE(
    "CLAP: an instance of a mono plug-in is given no cookie of another one",
    "[integration][clap][mono]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);
    gain_control(proc).setValue(0.25);
    report();
    playing p{*doc, proc};

    // When each starts, when a control changes, when the editor changes
    p.block(4);
    gain_control(proc).setValue(0.5);
    p.block(4);
    set_from_ui(proc, 0.75);
    for(int i = 0; i < 3; i++)
      p.block(4);

    CHECK(report().foreign_cookies == 0);
    p.stop();
  });
}

TEST_CASE(
    "CLAP: every instance of a mono plug-in follows score's controls and the first "
    "one's editor",
    "[integration][clap][mono]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);
    gain_control(proc).setValue(0.25);
    playing p{*doc, proc};

    auto all_at = [](const std::vector<double>& out, double v) {
      REQUIRE(out.size() == 4);
      for(double o : out)
        CHECK(o == Catch::Approx(v));
    };

    // The value the control had when the play started
    all_at(p.block(4), 0.25);

    // A control changed in score during the play
    gain_control(proc).setValue(0.5);
    all_at(p.block(4), 0.5);

    // The editor of the first instance
    set_from_ui(proc, 0.75);
    std::vector<double> out;
    for(int i = 0; i < 3; i++)
      out = p.block(4);
    all_at(out, 0.75);

    const auto e = report();
    CHECK(e.wrong_thread == 0);
    CHECK(e.foreign_cookies == 0);
    CHECK(e.bad_events == 0);
    p.stop();
  });
}

TEST_CASE(
    "CLAP: every instance of a mono plug-in starts with the state of the first one",
    "[integration][clap][mono]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);
    gain_control(proc).setValue(0.25);
    // What only the plug-in's state keeps, as a loaded model
    plugin_function<void(const clap_plugin_t*, double)>("clap_mono_gain_set_trim")(
        proc.handle()->plugin, 2.);
    playing p{*doc, proc};

    const auto out = p.block(4);
    REQUIRE(out.size() == 4);
    for(double o : out)
      CHECK(o == Catch::Approx(0.5));
    p.stop();
  });
}

TEST_CASE(
    "CLAP: the library of two processes of one plug-in is initialised once",
    "[integration][clap][library]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    const int before = report().initialised;
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    add_mono_gain(*doc);
    add_mono_gain(*doc);

    auto e = report();
    CHECK(e.initialised == before + 1);
    CHECK(e.double_init == 0);

    ctx.docManager.forceCloseDocument(ctx, *doc);
    spin(50);
    e = report();
    CHECK(e.initialised == before);
    CHECK(e.bad_state == 0);
    CHECK(e.late_calls == 0);
  });
}

TEST_CASE(
    "CLAP: a restart a plug-in asks for while stopped waits for the next play",
    "[integration][clap][restart]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);
    report();

    // It is activated when score plays, with the engine's rate and block size
    plugin_function<void(const clap_plugin_t*)>("clap_mono_gain_request_restart")(
        proc.handle()->plugin);
    spin(50);
    CHECK(report().activations == 0);

    // An inactive plug-in is flushed from the main thread
    gain_control(proc).setValue(0.3);
    CHECK(plugin_gain(proc) == Catch::Approx(0.3));
    CHECK(report().bad_state == 0);
  });
}

TEST_CASE(
    "CLAP: a stop deactivates a plug-in once it stopped processing",
    "[integration][clap][stop]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);
    const int before = report().active;
    {
      playing p{*doc, proc};
      for(int i = 0; i < 2; i++)
        p.block(2);
      p.stop();
    }
    spin(100);
    const auto e = report();
    CHECK(e.bad_state == 0);
    CHECK(e.active == before);
  });
}

TEST_CASE(
    "CLAP: a plug-in without note ports is sent no note when the notes are stopped",
    "[integration][clap][notes]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);
    playing p{*doc, proc};
    p.block(2);
    report();

    p.node->all_notes_off();
    p.block(2);
    CHECK(report().bad_events == 0);
    p.stop();
  });
}

TEST_CASE(
    "CLAP: a mono plug-in removed while it plays is stopped, then destroyed with its "
    "instances",
    "[integration][clap][library][mono]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    const auto before = report();
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);
    {
      playing p{*doc, proc};
      p.block(4);
      p.node.reset();
      CommandDispatcher<>{doc->context().commandStack}.submit(
          new Scenario::Command::RemoveProcessFromInterval{
              score::test::base_interval(*doc), proc.id()});
      for(int i = 0; i < 3; i++)
      {
        score::test::run_exec(p.plug);
        spin(20);
      }
      p.plug.baseScenario()->baseInterval().stop();
      score::test::run_exec(p.plug);
      p.plug.clear();
      spin(50);
    }

    const auto e = report();
    CHECK(e.bad_state == 0);
    CHECK(e.late_calls == 0);
    CHECK(e.wrong_thread == 0);
    CHECK(e.instances == before.instances);
    CHECK(e.active == before.active);
  });
}

namespace
{
//! A play of the whole life of a plug-in: its start, a control changed in
//! score, its editor, a restart it asks for and its own thread pool while it
//! plays, and its stop
void play_all_of_it(score::Document& doc, Clap::Model& proc, int channels)
{
  playing p{doc, proc};
  p.block(channels);
  gain_control(proc).setValue(0.4);
  p.block(channels);
  set_from_ui(proc, 0.6);
  p.block(channels);
  plugin_function<void(const clap_plugin_t*)>("clap_mono_gain_request_restart")(
      proc.handle()->plugin);
  spin(20);
  p.block(channels);
  p.stop();
}
}

TEST_CASE(
    "CLAP: the audio thread calls a plug-in only for what it may, from its start to "
    "its stop",
    "[integration][clap][threads]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_gain(*doc, QStringLiteral("io.ossia.test.stereo-gain"));
    plugin_function<void(const clap_plugin_t*)>("clap_mono_gain_use_thread_pool")(
        proc.handle()->plugin);
    plugin_function<int()>("clap_mono_gain_pool_tasks")();
    report();

    play_all_of_it(*doc, proc, 2);

    const auto e = report();
    CHECK(e.wrong_thread == 0);
    CHECK(e.bad_state == 0);
    // Its thread pool ran its tasks
    CHECK(plugin_function<int()>("clap_mono_gain_pool_tasks")() > 0);
  });
}

TEST_CASE(
    "CLAP: the audio thread calls the instances of a mono plug-in only for what it "
    "may, from their start to their stop",
    "[integration][clap][threads][mono]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_mono_gain(*doc);
    report();

    play_all_of_it(*doc, proc, 4);

    const auto e = report();
    CHECK(e.wrong_thread == 0);
    CHECK(e.bad_state == 0);
  });
}

// The clock's stop may find the audio thread in a tick that takes longer than
// it waits (a plug-in loading its samples in its process call): that tick runs
// the interval's stop, then the graph once more, while the main thread
// deactivates what stopped processing. A stopped plug-in does not start again
// there.
TEST_CASE(
    "CLAP: a plug-in its process stopped does not start processing again",
    "[integration][clap][state]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    for(auto id : {"io.ossia.test.stereo-gain", "io.ossia.test.mono-gain"})
    {
      INFO(id);
      auto* doc = score::test::new_document(ctx);
      REQUIRE(doc);
      auto& proc = *add_gain(*doc, QString::fromUtf8(id));
      report();
      {
        playing p{*doc, proc};
        p.block(2);

        auto& procs = p.plug.baseScenario()->baseInterval().processes();
        auto it = procs.find(proc.id());
        REQUIRE(it != procs.end());
        ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
        it->second->OSSIAProcess().stop();
        ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

        // What reset() does to the processes of the stopped interval
        proc.setExecuting(false);
        proc.resetExecution();
        spin(30);

        p.block(2);
        p.stop();
      }
      const auto e = report();
      CHECK(e.bad_state == 0);
      CHECK(e.wrong_thread == 0);
      ctx.docManager.forceCloseDocument(ctx, *doc);
    }
  });
}

// An entry that gives its plug-in factory for any id, preset discovery's
// included (SpectMorph): read as a preset discovery factory, its plug-in
// descriptors were taken for providers and the indexer for a host.
TEST_CASE(
    "CLAP: a plug-in factory given for preset discovery is not used as one",
    "[integration][clap][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(
        *doc, clap_uuid,
        QStringLiteral(CLAP_MONO_GAIN_ANY_FACTORY_PATH ":::io.ossia.test.stereo-gain"));
    REQUIRE(proc);
    REQUIRE(static_cast<Clap::Model*>(proc)->handle());
    CHECK(proc->builtinPresets().empty());
  });
}

// A restart the plug-in asks for while a process of a stopped interval, or one
// the main thread did not see start yet, is processing: the stop that ends its
// processing deactivates it, not the request.
TEST_CASE(
    "CLAP: a restart asked for while processing does not deactivate the plug-in",
    "[integration][clap][state]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_gain(*doc, QStringLiteral("io.ossia.test.stereo-gain"));
    report();
    {
      playing p{*doc, proc};
      p.block(2);
      proc.setExecuting(false);
      plugin_function<void(const clap_plugin_t*)>("clap_mono_gain_request_restart")(
          proc.handle()->plugin);
      spin(30);
      p.block(2);
      p.stop();
    }
    const auto e = report();
    CHECK(e.bad_state == 0);
    CHECK(e.wrong_thread == 0);
  });
}

// A control moved while the node of its process runs, its process not marked
// executing (as after a reset): an active plug-in is not flushed from the main
// thread.
TEST_CASE(
    "CLAP: a control moved while the plug-in is active is not flushed from the main "
    "thread",
    "[integration][clap][params]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = *add_gain(*doc, QStringLiteral("io.ossia.test.stereo-gain"));
    report();
    {
      playing p{*doc, proc};
      p.block(2);
      proc.setExecuting(false);
      gain_control(proc).setValue(0.3);
      spin(20);
      p.block(2);
      p.stop();
    }
    const auto e = report();
    CHECK(e.bad_state == 0);
    CHECK(e.wrong_thread == 0);
  });
}
