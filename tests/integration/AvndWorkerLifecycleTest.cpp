// The avendish object contract that out-of-tree plug-in addons depend on.
//
// Such addons are avendish/halp objects that score loads through
// score-plugin-avnd / Crousti. They need not touch the device tree or the
// transport: what they lean on is the *object* contract -- the worker thread,
// the callback and value outlets, `struct messages`, `prepare()`, and a
// cpu_buffer_output whose payload is resized on every tick. No such addon is
// installed here, so every object below is purpose-built in this file to have
// the shape a real one has:
//
//  * WorkerObject     -- an object whose worker runs a model off-thread:
//                      `struct worker { std::function<void(shared_ptr<Job>)>
//                      request; static ... work(shared_ptr<Job>); }` plus a
//                      halp::callback outlet.
//  * ContainerOutputs -- container payloads on value outlets, as in
//                      `val_port<"Tokens", std::vector<std::string>>`,
//                      `val_port<"Timestamps", std::vector<float>>` and
//                      `val_port<"Embedding", std::vector<float>>`.
//  * MessageInlets    -- a `struct messages` enroll/remove/clear, which score
//                      maps to three Process::ValueInlet dispatched by
//                      oscr::message_processor.
//  * PointCloud       -- an object publishing a point cloud whose size changes
//                      every tick: `halp::cpu_buffer_output<"Points">` +
//                      `create<float>(n)` / `upload()` per tick, with n
//                      changing every tick (a sensor sees a varying number of
//                      points).
//  * PrepareProbe     -- `prepare(halp::setup)` under a changing buffer size
//                      and sample rate, which is where an object allocates its
//                      accumulators and its Job.
//
// The exec node is driven the way the audio graph drives it: outlets cleared
// (ossia::graph_util::init_node does that before every node run), run() called
// with a real token and execution state, and the execution queue drained by
// hand (runAllCommands) exactly where the execution tick drains it.
// Nothing plays: the tests are deterministic except where a real thread pool is
// the thing under test, and there the waits are bounded and asserted.

#include <Process/ExecutionSetup.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QElapsedTimer>

#include <Crousti/Executor.hpp>
#include <Crousti/ProcessModel.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#if defined(SCORE_PLUGIN_GFX) && SCORE_PLUGIN_GFX
#include <Gfx/Graph/NodeRenderer.hpp>

#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <halp/texture.hpp>
#include <score_test/Gfx.hpp>
#endif
#include <malloc.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using Catch::Approx;

#if defined(SCORE_PLUGIN_GFX) && SCORE_PLUGIN_GFX
//! Captured at static-init time, before any test runs: whether the process was
//! started with an explicit QPA platform. See the [gfx] case at the bottom.
static const bool g_qpa_platform_was_set = qEnvironmentVariableIsSet("QT_QPA_PLATFORM");
#endif

namespace
{
// Harness
constexpr int64_t flicks_per_sample(int rate)
{
  return 705600000 / rate;
}

ossia::token_request make_token(int tick, int rate, int buffer)
{
  const int64_t per_buffer = flicks_per_sample(rate) * buffer;
  ossia::token_request tk;
  tk.prev_date = ossia::time_value{per_buffer * tick};
  tk.date = ossia::time_value{per_buffer * (tick + 1)};
  tk.parent_duration = ossia::time_value{per_buffer * 4096};
  tk.speed = 1.;
  tk.tempo = 120.;
  tk.signature = ossia::time_signature{4, 4};
  tk.start_sample = 0;
  tk.length_sample = buffer;
  return tk;
}

//! Runs what the exec thread would run. The exec commands check that they are
//! on an audio thread, so the test thread poses as one for the duration.
void run_exec(Execution::DocumentPlugin& plug)
{
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  plug.runAllCommands();
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
}

void spin(int ms = 0)
{
  QApplication::processEvents();
  if(ms > 0)
  {
    QElapsedTimer t;
    t.start();
    while(t.elapsed() < ms)
      QApplication::processEvents(QEventLoop::AllEvents, 5);
  }
  QApplication::processEvents();
}

//! Spin the Qt event loop and the execution queue until `pred` holds, at most
//! `ms`. Returns whether it held: callers assert on that, so a hang shows up as
//! a failed expectation rather than as a stuck test.
template <typename F>
bool spin_until(Execution::DocumentPlugin& plug, F pred, int ms = 5000)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
  {
    QApplication::processEvents(QEventLoop::AllEvents, 2);
    run_exec(plug);
    if(pred())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  run_exec(plug);
  return pred();
}

//! What ossia::graph_util::init_node does to a node's outlets immediately
//! before running it.
void clear_outputs(ossia::graph_node& node)
{
  for(auto* o : node.root_outputs())
  {
    if(auto* v = o->target<ossia::value_port>())
      v->get_data().clear();
    else if(auto* a = o->target<ossia::audio_port>())
      for(auto& chan : *a)
        chan.clear();
  }
}

//! What ossia::graph_util::teardown_inlet does after the node ran: a value
//! that was consumed for one tick is gone on the next one.
void clear_inputs(ossia::graph_node& node)
{
  for(auto* i : node.root_inputs())
  {
    if(auto* v = i->target<ossia::value_port>())
      v->get_data().clear();
    else if(auto* m = i->target<ossia::midi_port>())
      m->messages.clear();
  }
}

//! Execution::ProcessComponent::cleanup() does not free the exec node: it hands
//! it to the GC queue, which the app drains from
//! DocumentPlugin::processEditCommands on a timer. Drain it by hand so a
//! destroyed process is actually gone by the time the test looks.
void drain_gc(Execution::DocumentPlugin& plug)
{
  auto& data = *plug.contextData();
  Execution::GCCommand gc;
  Execution::ExecutionCommand cmd;
  bool any = true;
  while(any)
  {
    any = false;
    if(data.m_editionQueue.try_dequeue(cmd))
    {
      cmd();
      any = true;
    }
    if(data.m_gcQueue.try_dequeue(gc))
    {
      gc();
      any = true;
    }
  }
}

std::vector<ossia::value> values_of(ossia::graph_node& node, int outlet)
{
  std::vector<ossia::value> res;
  auto* p = node.root_outputs()[outlet]->target<ossia::value_port>();
  REQUIRE(p);
  for(auto& tv : p->get_data())
    res.push_back(tv.value);
  return res;
}

std::vector<int64_t> timestamps_of(ossia::graph_node& node, int outlet)
{
  std::vector<int64_t> res;
  auto* p = node.root_outputs()[outlet]->target<ossia::value_port>();
  REQUIRE(p);
  for(auto& tv : p->get_data())
    res.push_back(tv.timestamp);
  return res;
}

void write_value(ossia::graph_node& node, int inlet, ossia::value v)
{
  auto* p = node.root_inputs()[inlet]->target<ossia::value_port>();
  REQUIRE(p);
  p->write_value(std::move(v), 0);
}

Execution::DocumentPlugin& load_execution(score::Document& doc)
{
  auto& plug = doc.context().plugin<Execution::DocumentPlugin>();
  plug.reload(true, score::test::base_interval(doc));
  run_exec(plug);
  return plug;
}

//! A test-local avnd object mounted the way an interval component would mount
//! it: a real oscr::ProcessModel<T>, a real oscr::Executor<T> (which is what
//! installs worker.request, the control connections and the callback outlets),
//! registered in the SetupContext. Destroying it runs the same teardown the
//! app runs when a process goes away mid-playback.
template <typename T>
struct mounted
{
  Execution::DocumentPlugin& plug;
  oscr::ProcessModel<T>* model{};
  std::shared_ptr<oscr::Executor<T>> comp;
  int tick_index{};

  mounted(score::Document& doc, Execution::DocumentPlugin& p, int id)
      : plug{p}
  {
    model = new oscr::ProcessModel<T>{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{id}, doc.context(),
        &score::test::base_interval(doc)};
    comp = std::make_shared<oscr::Executor<T>>(*model, plug.context(), nullptr);
    REQUIRE(comp->node);
    plug.context().setup.register_node(*model, comp->node);
    run_exec(plug);
  }

  ~mounted()
  {
    if(comp)
    {
      comp->cleanup();
      run_exec(plug);
      comp.reset();
      drain_gc(plug);
    }
    delete model;
    spin();
    drain_gc(plug);
  }

  std::weak_ptr<ossia::graph_node> weak_node() const { return comp->node; }

  oscr::safe_node<T>& node() { return static_cast<oscr::safe_node<T>&>(*comp->node); }
  T& object() { return node().impl.effect; }

  void tick(int rate = 48000, int buffer = 64)
  {
    auto& n = *comp->node;
    clear_outputs(n);
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    n.run(
        make_token(tick_index++, rate, buffer),
        ossia::exec_state_facade{plug.context().execState.get()});
    clear_inputs(n);
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  }
};

//! Tick the node -- pumping the Qt hop and the execution queue in between --
//! until `pred` holds, and stop on the tick that made it true, so the caller
//! reads the ports of that very tick. A worker result is handed to the node
//! from the Qt main thread and applied at the beginning of the node's next
//! tick (Crousti's node_with_worker), which is what this waits for.
//! Bounded: a result that never lands is a failed expectation, not a hang.
template <typename T, typename F>
bool tick_until(mounted<T>& m, Execution::DocumentPlugin& plug, F pred, int ms = 5000)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
  {
    QApplication::processEvents(QEventLoop::AllEvents, 2);
    run_exec(plug);
    m.tick();
    if(pred())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return pred();
}

// 1. worker thread + callback outlet, the shape an off-thread model uses
struct worker_probe
{
  std::atomic<int> work_calls{0};
  std::atomic<int> closure_calls{0};
  std::atomic<int> work_entered{0};
  std::atomic<bool> gate{true};
  std::atomic<unsigned> magic_seen{0};
  std::atomic<bool> gate_timed_out{false};

  std::mutex mut;
  std::vector<std::thread::id> work_threads;
  std::vector<int> closure_order;
  std::thread::id request_thread{};
  std::thread::id closure_thread{};

  void reset()
  {
    work_calls = 0;
    closure_calls = 0;
    work_entered = 0;
    gate = true;
    magic_seen = 0;
    gate_timed_out = false;
    std::lock_guard l{mut};
    work_threads.clear();
    closure_order.clear();
    request_thread = {};
    closure_thread = {};
  }
};
worker_probe g_worker;

//! The value the closure reads out of the object. A closure that lands on a
//! destroyed object would read poisoned or reused heap instead (the test turns
//! on glibc's M_PERTURB for that).
constexpr unsigned probe_magic = 0xC0FFEE42u;

struct WorkerObject
{
  halp_meta(name, "Test worker")
  halp_meta(c_name, "score_test_worker")
  halp_meta(category, "Test")
  halp_meta(author, "score tests")
  halp_meta(description, "worker thread + callback outlet")
  halp_meta(uuid, "b1e0f3d6-6f5a-4a21-9b0e-2c7a8d4e1f30")

  struct
  {
    halp::spinbox_i32<"Submit", halp::range{0, 4096, 0}> submit;
  } inputs;

  struct
  {
    halp::callback<"Result", int> result;
    halp::val_port<"Last", int> last;
  } outputs;

  struct Job
  {
    int payload{};
    int doubled{};
  };

  struct worker
  {
    std::function<void(std::shared_ptr<Job>)> request;

    static std::function<void(WorkerObject&)> work(std::shared_ptr<Job> job)
    {
      g_worker.work_calls++;
      {
        std::lock_guard l{g_worker.mut};
        g_worker.work_threads.push_back(std::this_thread::get_id());
      }
      g_worker.work_entered++;

      // Bounded: a gate that never opens is a failed expectation, not a hang.
      QElapsedTimer t;
      t.start();
      while(!g_worker.gate.load(std::memory_order_acquire))
      {
        if(t.elapsed() > 10000)
        {
          g_worker.gate_timed_out = true;
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      job->doubled = job->payload * 2;
      return [job](WorkerObject& self) {
        // Back on the DSP thread, through the execution queue.
        g_worker.closure_calls++;
        g_worker.closure_thread = std::this_thread::get_id();
        g_worker.magic_seen = self.m_magic;
        {
          std::lock_guard l{g_worker.mut};
          g_worker.closure_order.push_back(job->doubled);
        }
        self.m_done.push_back(job->doubled);
      };
    }
  } worker;

  void operator()(int frames)
  {
    if(inputs.submit.value != m_last_submitted)
    {
      m_last_submitted = inputs.submit.value;
      g_worker.request_thread = std::this_thread::get_id();
      auto job = std::make_shared<Job>();
      job->payload = m_last_submitted;
      worker.request(job);
    }

    // Emitted from run(), the way an object drains its pending results.
    for(int v : m_done)
    {
      outputs.result(v);
      outputs.last.value = v;
    }
    m_done.clear();
  }

  unsigned m_magic = probe_magic;
  int m_last_submitted{};
  std::vector<int> m_done;
};

// 1b. the same worker contract, but the result writes the outlets itself
struct direct_probe
{
  std::atomic<int> work_calls{0};
  std::atomic<int> closure_calls{0};
  std::thread::id closure_thread{};

  void reset()
  {
    work_calls = 0;
    closure_calls = 0;
    closure_thread = {};
  }
};
direct_probe g_direct;

//! The usual object stashes what the worker returned and emits it from
//! operator(). This object is the other form, the one avendish's worker
//! contract offers directly: the result closure is invoked "back in the
//! processor DSP thread" (Crousti's connect_worker), writes the callback outlet
//! and the value outlet itself, and never touches object state that operator()
//! would re-emit.
struct WorkerDirectObject
{
  halp_meta(name, "Test worker direct")
  halp_meta(c_name, "score_test_worker_direct")
  halp_meta(category, "Test")
  halp_meta(author, "score tests")
  halp_meta(description, "worker result writes the outlets from the closure")
  halp_meta(uuid, "f7c21a48-95d6-4b03-8e17-6ad4b2c95e71")

  struct
  {
    halp::spinbox_i32<"Submit", halp::range{0, 4096, 0}> submit;
  } inputs;

  struct
  {
    halp::callback<"Result", int> result;
    halp::val_port<"Last", int> last;
  } outputs;

  struct Job
  {
    int payload{};
  };

  struct worker
  {
    std::function<void(std::shared_ptr<Job>)> request;

    static std::function<void(WorkerDirectObject&)> work(std::shared_ptr<Job> job)
    {
      g_direct.work_calls++;
      return [job](WorkerDirectObject& self) {
        g_direct.closure_calls++;
        g_direct.closure_thread = std::this_thread::get_id();
        // The whole point: written from the result, not stashed for operator().
        self.outputs.result(job->payload * 2);
        self.outputs.last.value = job->payload * 2;
      };
    }
  } worker;

  void operator()(int frames)
  {
    if(inputs.submit.value != m_last_submitted)
    {
      m_last_submitted = inputs.submit.value;
      auto job = std::make_shared<Job>();
      job->payload = m_last_submitted;
      worker.request(job);
    }
    ticks++;
  }

  int m_last_submitted{};
  int ticks{};
};

// 2. container payloads on value outlets
struct ContainerOutputs
{
  halp_meta(name, "Test containers")
  halp_meta(c_name, "score_test_containers")
  halp_meta(category, "Test")
  halp_meta(author, "score tests")
  halp_meta(description, "vector<string> / vector<float> value outlets")
  halp_meta(uuid, "2d4c7b91-58e3-4ff1-8a06-9e3b1c0d7a58")

  struct
  {
    halp::spinbox_i32<"Count", halp::range{0, 64, 3}> count;
  } inputs;

  struct
  {
    halp::val_port<"Tokens", std::vector<std::string>> tokens;
    halp::val_port<"Timestamps", std::vector<float>> timestamps;
  } outputs;

  void operator()(int frames)
  {
    const int n = inputs.count.value;
    outputs.tokens.value.clear();
    outputs.timestamps.value.clear();
    for(int i = 0; i < n; i++)
    {
      outputs.tokens.value.push_back("tok" + std::to_string(i));
      outputs.timestamps.value.push_back(0.25f * float(i));
    }
  }
};

// 3. struct messages, as an object with control messages declares them
struct MessageInlets
{
  halp_meta(name, "Test messages")
  halp_meta(c_name, "score_test_messages")
  halp_meta(category, "Test")
  halp_meta(author, "score tests")
  halp_meta(description, "enroll / remove / clear message inlets")
  halp_meta(uuid, "6a8f2c05-3b71-4d9e-ae12-5f7d0b4c9e63")

  struct
  {
    halp::knob_f32<"Threshold"> threshold;
  } inputs;

  struct
  {
    halp::val_port<"Count", int> count;
  } outputs;

  struct messages
  {
    struct
    {
      halp_meta(name, "enroll")
      void operator()(MessageInlets& self, std::string name) { self.enroll(name); }
    } enroll;

    struct
    {
      halp_meta(name, "remove")
      void operator()(MessageInlets& self, std::string name) { self.remove(name); }
    } remove;

    struct
    {
      halp_meta(name, "clear")
      void operator()(MessageInlets& self) { self.clear(); }
    } clear;
  };

  void enroll(const std::string& n) { enrolled.push_back(n); }
  void remove(const std::string& n)
  {
    std::erase(enrolled, n);
    removed.push_back(n);
  }
  void clear()
  {
    enrolled.clear();
    clears++;
  }

  void operator()(int frames) { outputs.count.value = int(enrolled.size()); }

  std::vector<std::string> enrolled;
  std::vector<std::string> removed;
  int clears{};
};

// 4. a point-cloud shape: a cpu_buffer_output resized on every tick
struct PointCloud
{
  halp_meta(name, "Test points")
  halp_meta(c_name, "score_test_points")
  halp_meta(category, "Test")
  halp_meta(author, "score tests")
  halp_meta(description, "point cloud on a cpu_buffer_output, resized per tick")
  halp_meta(uuid, "0c5b9a72-4e18-4b6d-97af-13d8e6205c74")

  struct
  {
    halp::spinbox_i32<"Points", halp::range{0, 65536, 0}> points;
  } inputs;

  struct
  {
    halp::cpu_buffer_output<"Points"> points;
  } outputs;

  void operator()()
  {
    const int n = inputs.points.value;
    auto span = outputs.points.create<float>(int64_t(n) * 3);
    for(int i = 0; i < n; i++)
    {
      span[i * 3 + 0] = float(i);
      span[i * 3 + 1] = float(i) + 0.5f;
      span[i * 3 + 2] = float(-i);
    }
    outputs.points.upload();
    ticks++;
  }

  int ticks{};
};

//! score installs exactly this shape of callback on the port: it is the only
//! thing that sees the payload, so it is what a consumer's byte count comes
//! from.
struct upload_record
{
  int64_t offset{};
  int64_t bytesize{};
  std::vector<float> floats;
};

// 5. prepare() under a changing buffer size / sample rate
struct prepare_record
{
  int input_channels{};
  int output_channels{};
  int frames_per_buffer{};
  double rate{};
};

struct PrepareProbe
{
  halp_meta(name, "Test prepare")
  halp_meta(c_name, "score_test_prepare")
  halp_meta(category, "Test")
  halp_meta(author, "score tests")
  halp_meta(description, "prepare(halp::setup) under a changing configuration")
  halp_meta(uuid, "9f13a6c4-7d20-4e55-b8c1-0a6e2f4b7d19")

  struct
  {
    halp::dynamic_audio_bus<"In", float> audio;
    halp::knob_f32<"Gain"> gain;
  } inputs;

  struct
  {
    halp::val_port<"Rate", float> rate;
    halp::val_port<"Frames", int> frames;
  } outputs;

  void prepare(halp::setup info)
  {
    setups.push_back(
        {info.input_channels, info.output_channels, info.frames, info.rate});
    // The usual shape of a prepare(): the buffers are sized from the setup,
    // so a wrong setup is a wrong allocation.
    scratch.assign(std::size_t(info.frames) * 4, 0.f);
    host_rate = info.rate;
  }

  void operator()(int frames)
  {
    outputs.rate.value = float(host_rate);
    outputs.frames.value = frames;
    seen_channels = inputs.audio.channels;
    if(!scratch.empty() && frames > 0)
      scratch[0] = float(frames);
  }

  std::vector<prepare_record> setups;
  std::vector<float> scratch;
  double host_rate{};
  int seen_channels{};
};
}

TEST_CASE(
    "A worker result reaches a callback outlet, on the thread that runs the node",
    "[avnd][worker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = load_execution(*doc);

    g_worker.reset();
    mounted<WorkerObject> m{*doc, plug, 8801};

    // inlets: [Submit], outlets: [Result (callback), Last]
    REQUIRE(m.model->inlets().size() == 1);
    REQUIRE(m.model->outlets().size() == 2);

    const auto ui_thread = std::this_thread::get_id();

    // Nothing submitted yet: the first tick is silent.
    m.tick();
    CHECK(values_of(m.node(), 0).empty());
    CHECK(g_worker.work_calls == 0);

    // Submit 21 -> the worker doubles it to 42.
    m.object().inputs.submit.value = 21;
    m.tick();
    CHECK(g_worker.request_thread == ui_thread); // request() is called from run()
    // The result is NOT there yet: the work is off-thread.
    CHECK(values_of(m.node(), 0).empty());

    // The work happens off-thread and the result is handed back to the node,
    // which applies it at the beginning of its next tick: pumping the queues
    // without ticking delivers nothing.
    REQUIRE(spin_until(plug, [] { return g_worker.work_calls.load() == 1; }));
    CHECK_FALSE(g_worker.gate_timed_out);
    {
      std::lock_guard l{g_worker.mut};
      REQUIRE(g_worker.work_threads.size() == 1);
      // work() ran on a pool thread, not on the one that ticks the node
      CHECK(g_worker.work_threads[0] != ui_thread);
    }
    CHECK(values_of(m.node(), 0).empty());

    // The result is applied at the start of this tick, and the object emits
    // what it stashed from it in the very same run().
    REQUIRE(tick_until(m, plug, [] { return g_worker.closure_calls.load() == 1; }));

    // The closure ran against the live object...
    CHECK(g_worker.magic_seen == probe_magic);
    // ... on the thread that ticks the node, not on the pool thread.
    CHECK(g_worker.closure_thread == ui_thread);

    auto vals = values_of(m.node(), 0);
    REQUIRE(vals.size() == 1);
    CHECK(vals[0] == ossia::value{42});
    CHECK(timestamps_of(m.node(), 0) == std::vector<int64_t>{0});
    // The plain value outlet carries it too
    auto last = values_of(m.node(), 1);
    REQUIRE(last.size() == 1);
    CHECK(last[0] == ossia::value{42});

    // And it is not re-emitted forever
    m.tick();
    CHECK(values_of(m.node(), 0).empty());
  });
}

TEST_CASE(
    "Several worker results emitted in one tick keep their order on the outlet",
    "[avnd][worker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = load_execution(*doc);

    g_worker.reset();
    g_worker.gate = false; // hold the pool threads inside work()
    mounted<WorkerObject> m{*doc, plug, 8802};

    for(int k : {1, 2, 3})
    {
      m.object().inputs.submit.value = k;
      m.tick();
    }
    // All three are in flight, none delivered
    REQUIRE(spin_until(plug, [] { return g_worker.work_entered.load() == 3; }));
    CHECK(g_worker.closure_calls == 0);

    g_worker.gate = true;
    REQUIRE(spin_until(plug, [] { return g_worker.work_calls.load() == 3; }));
    CHECK_FALSE(g_worker.gate_timed_out);

    // The three results are handed back to the node (pool thread -> Qt main
    // thread); not one of them is applied while the node does not tick.
    spin(50);
    CHECK(g_worker.closure_calls == 0);

    // All three are applied at the start of this tick, and the object emits
    // them from the same run().
    m.tick();
    REQUIRE(g_worker.closure_calls == 3);

    std::vector<int> expected;
    {
      std::lock_guard l{g_worker.mut};
      expected = g_worker.closure_order;
    }
    REQUIRE(expected.size() == 3);

    auto vals = values_of(m.node(), 0);
    REQUIRE(vals.size() == 3);
    std::vector<int> got;
    for(auto& v : vals)
      got.push_back(*v.target<int>());
    // Order preserved from the node's result queue through do_callback into the port
    CHECK(got == expected);
    // and the set is the three doubled payloads, none lost, none duplicated
    auto sorted = got;
    std::sort(sorted.begin(), sorted.end());
    CHECK(sorted == std::vector<int>{2, 4, 6});
    // All at the tick's start frame
    CHECK(timestamps_of(m.node(), 0) == std::vector<int64_t>{0, 0, 0});
  });
}

TEST_CASE(
    "A worker job still in flight when the process is destroyed cannot write into it",
    "[avnd][worker][teardown]")
{
  // glibc: fill freed blocks (and fresh allocations) with a poison byte, so a
  // closure that landed on a destroyed object would read poison rather than the
  // value that happened to still be there.
  mallopt(M_PERTURB, 0x5A);

  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = load_execution(*doc);

    constexpr int cycles = 25;
    int destroyed_in_flight = 0;
    int freed = 0;

    for(int i = 0; i < cycles; i++)
    {
      g_worker.reset();
      g_worker.gate = false;

      std::weak_ptr<ossia::graph_node> weak;
      {
        mounted<WorkerObject> m{*doc, plug, 8900 + i};
        weak = m.weak_node();
        m.object().inputs.submit.value = i + 1;
        m.tick();
        // The pool thread is inside work(), holding only the Job shared_ptr
        REQUIRE(spin_until(plug, [] { return g_worker.work_entered.load() == 1; }));
      }
      // The process, its exec node and the object are really gone here -- the
      // weak_ptr the worker closure is guarded by has expired -- while work()
      // is still running on the pool thread.
      if(weak.expired())
        freed++;
      REQUIRE(weak.expired());

      g_worker.gate = true;

      // Give the pool thread, the Qt hop and the execution queue every chance
      // to deliver into the freed object.
      REQUIRE(spin_until(plug, [] { return g_worker.work_calls.load() == 1; }));
      for(int k = 0; k < 20; k++)
      {
        spin();
        run_exec(plug);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      CHECK_FALSE(g_worker.gate_timed_out);
      // The result closure was dropped instead of running against freed state
      CHECK(g_worker.closure_calls == 0);
      CHECK(g_worker.magic_seen == 0u);
      if(g_worker.closure_calls == 0)
        destroyed_in_flight++;
    }

    CHECK(freed == cycles);
    CHECK(destroyed_in_flight == cycles);
  });

  mallopt(M_PERTURB, 0);
}

TEST_CASE(
    "A worker result that writes an outlet itself is delivered on the tick it is "
    "applied on",
    "[avnd][worker]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = load_execution(*doc);

    g_direct.reset();
    mounted<WorkerDirectObject> m{*doc, plug, 8820};
    const auto ui_thread = std::this_thread::get_id();

    // Nothing submitted: nothing anywhere.
    m.tick();
    CHECK(values_of(m.node(), 0).empty());
    CHECK(g_direct.work_calls == 0);

    m.object().inputs.submit.value = 21;
    m.tick();
    CHECK(values_of(m.node(), 0).empty());
    REQUIRE(spin_until(plug, [] { return g_direct.work_calls.load() == 1; }));

    // tick_until stops on the tick the result was applied on: that is the
    // tick whose outlets the closure's write has to reach.
    REQUIRE(tick_until(m, plug, [] { return g_direct.closure_calls.load() == 1; }));
    // The result ran on the thread that ticks the node...
    CHECK(g_direct.closure_thread == ui_thread);
    // ... and what it wrote to the callback outlet is on the port of that very
    // tick, at a frame index inside it -- not wiped by the outlet clear that
    // ossia::graph_util::init_node does just before the node runs.
    auto callback = values_of(m.node(), 0);
    REQUIRE(callback.size() == 1);
    CHECK(callback[0] == ossia::value{42});
    CHECK(timestamps_of(m.node(), 0) == std::vector<int64_t>{0});
    // The value outlet it wrote carries it too.
    auto last = values_of(m.node(), 1);
    REQUIRE(last.size() == 1);
    CHECK(last[0] == ossia::value{42});

    // Emitted once: the next tick has nothing new on the callback outlet.
    m.tick();
    CHECK(values_of(m.node(), 0).empty());
    CHECK(g_direct.closure_calls == 1);

    // Three more results, all written straight from their closures: each one
    // is delivered, none is lost.
    for(int k : {2, 3, 4})
    {
      m.object().inputs.submit.value = k;
      m.tick();
    }
    REQUIRE(spin_until(plug, [] { return g_direct.work_calls.load() == 4; }));

    std::vector<int> delivered;
    for(int k = 0; k < 500 && int(delivered.size()) < 3; k++)
    {
      spin();
      run_exec(plug);
      m.tick();
      for(auto& v : values_of(m.node(), 0))
        delivered.push_back(*v.target<int>());
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::sort(delivered.begin(), delivered.end());
    CHECK(delivered == std::vector<int>{4, 6, 8});
    CHECK(g_direct.closure_calls == 4);
  });
}

TEST_CASE(
    "A worker result queued for a node that is destroyed before its next tick is "
    "dropped",
    "[avnd][worker][teardown]")
{
  mallopt(M_PERTURB, 0x5A);

  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = load_execution(*doc);

    // A result that has already made it all the way to the node, but whose
    // node never ticks again because the process is destroyed: it must die
    // with the node instead of running against freed state.
    for(int i = 0; i < 10; i++)
    {
      g_direct.reset();
      std::weak_ptr<ossia::graph_node> weak;
      {
        mounted<WorkerDirectObject> m{*doc, plug, 8830 + i};
        weak = m.weak_node();
        m.object().inputs.submit.value = i + 1;
        m.tick();
        REQUIRE(spin_until(plug, [] { return g_direct.work_calls.load() == 1; }));
        // Let the Qt hop hand the result to the node -- but never tick again.
        spin(20);
        CHECK(g_direct.closure_calls == 0);
      }
      REQUIRE(weak.expired());

      for(int k = 0; k < 20; k++)
      {
        spin();
        run_exec(plug);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      CHECK(g_direct.closure_calls == 0);
    }
  });

  mallopt(M_PERTURB, 0);
}

TEST_CASE(
    "Container payloads round-trip from a value outlet to a score port",
    "[avnd][value]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = load_execution(*doc);

    mounted<ContainerOutputs> m{*doc, plug, 8810};
    REQUIRE(m.model->outlets().size() == 2);

    auto tokens = [&] { return values_of(m.node(), 0); };
    auto stamps = [&] { return values_of(m.node(), 1); };

    // Default count is 3
    m.tick();
    {
      auto t = tokens();
      REQUIRE(t.size() == 1);
      auto* list = t[0].target<std::vector<ossia::value>>();
      REQUIRE(list);
      REQUIRE(list->size() == 3);
      CHECK(*(*list)[0].target<std::string>() == "tok0");
      CHECK(*(*list)[1].target<std::string>() == "tok1");
      CHECK(*(*list)[2].target<std::string>() == "tok2");

      auto s = stamps();
      REQUIRE(s.size() == 1);
      auto* fl = s[0].target<std::vector<ossia::value>>();
      REQUIRE(fl);
      REQUIRE(fl->size() == 3);
      CHECK(ossia::convert<float>((*fl)[1]) == Approx(0.25f));
      CHECK(ossia::convert<float>((*fl)[2]) == Approx(0.5f));
    }

    // Grow
    m.object().inputs.count.value = 7;
    m.tick();
    {
      auto t = tokens();
      REQUIRE(t.size() == 1);
      REQUIRE(t[0].target<std::vector<ossia::value>>()->size() == 7);
      CHECK(
          *(*t[0].target<std::vector<ossia::value>>())[6].target<std::string>() == "tok6");
    }

    // Shrink: the port must carry exactly the new payload, with no leftovers
    // from the longer one.
    m.object().inputs.count.value = 2;
    m.tick();
    {
      auto t = tokens();
      REQUIRE(t.size() == 1);
      auto* list = t[0].target<std::vector<ossia::value>>();
      REQUIRE(list);
      REQUIRE(list->size() == 2);
      CHECK(*(*list)[1].target<std::string>() == "tok1");

      auto s = stamps();
      REQUIRE(s[0].target<std::vector<ossia::value>>()->size() == 2);
    }

    // Empty: an empty container is still a value, not a missing one
    m.object().inputs.count.value = 0;
    m.tick();
    {
      auto t = tokens();
      REQUIRE(t.size() == 1);
      auto* list = t[0].target<std::vector<ossia::value>>();
      REQUIRE(list);
      CHECK(list->empty());
    }
  });
}

TEST_CASE("struct messages dispatch to the right inlet", "[avnd][messages]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = load_execution(*doc);

    mounted<MessageInlets> m{*doc, plug, 8820};

    // inlets: three message inlets (in declaration order) then the control
    REQUIRE(m.model->inlets().size() == 4);
    CHECK(m.model->inlets()[0]->name() == "enroll");
    CHECK(m.model->inlets()[1]->name() == "remove");
    CHECK(m.model->inlets()[2]->name() == "clear");
    CHECK(m.model->inlets()[3]->name() == "Threshold");
    REQUIRE(m.node().root_inputs().size() == 4);

    auto& obj = m.object();

    write_value(m.node(), 0, std::string("bob"));
    write_value(m.node(), 0, std::string("alice"));
    m.tick();
    CHECK(obj.enrolled == std::vector<std::string>{"bob", "alice"});
    CHECK(obj.removed.empty());
    CHECK(obj.clears == 0);
    // The count outlet reflects it
    {
      auto c = values_of(m.node(), 0);
      REQUIRE(c.size() == 1);
      CHECK(*c[0].target<int>() == 2);
    }

    // remove goes to the second inlet, not the first
    write_value(m.node(), 1, std::string("bob"));
    m.tick();
    CHECK(obj.enrolled == std::vector<std::string>{"alice"});
    CHECK(obj.removed == std::vector<std::string>{"bob"});
    CHECK(obj.clears == 0);

    // clear takes no argument: any value on the third inlet fires it
    write_value(m.node(), 2, ossia::impulse{});
    m.tick();
    CHECK(obj.enrolled.empty());
    CHECK(obj.clears == 1);
    CHECK(obj.removed == std::vector<std::string>{"bob"});

    // A message inlet consumed for one tick does not fire again on the next
    m.tick();
    CHECK(obj.clears == 1);

    // Two messages on two different inlets in the same tick
    write_value(m.node(), 0, std::string("carol"));
    write_value(m.node(), 2, ossia::impulse{});
    m.tick();
    CHECK(obj.clears == 2);
    // enroll ran, then clear wiped it: both were dispatched
    CHECK(obj.enrolled.empty());

    write_value(m.node(), 0, std::string("dave"));
    m.tick();
    CHECK(obj.enrolled == std::vector<std::string>{"dave"});
  });
}

TEST_CASE(
    "A cpu_buffer_output resized every tick reports the exact payload each time",
    "[avnd][buffer]")
{
  // No score document needed: this is the payload boundary score's
  // buffer_outputs_storage plugs into. The object is created and destroyed
  // twice to cover the recreate path.
  std::vector<upload_record> log;

  for(int cycle = 0; cycle < 2; cycle++)
  {
    PointCloud obj;
    obj.outputs.points.buffer.upload
        = [&log](const char* data, int64_t offset, int64_t bytesize) {
      upload_record r;
      r.offset = offset;
      r.bytesize = bytesize;
      r.floats.resize(std::size_t(bytesize) / sizeof(float));
      std::memcpy(r.floats.data(), data, std::size_t(bytesize));
      log.push_back(std::move(r));
    };

    // A point cloud that grows, shrinks hard, grows again, empties, comes back.
    const std::vector<int> counts{4, 10, 2, 7, 0, 3};
    for(int n : counts)
    {
      obj.inputs.points.value = n;
      obj();

      auto& buf = obj.outputs.points.buffer;
      // upload() marks it; this is what score's uploadOutputBuffer keys on
      REQUIRE(buf.changed);
      // The byte count is exactly this tick's payload, on every tick, growing
      // AND shrinking.
      CHECK(buf.byte_size == int64_t(n) * 3 * int64_t(sizeof(float)));
      REQUIRE((buf.raw_data != nullptr || n == 0));

      // Shrinking must not leave the previous, longer cloud visible: every
      // byte inside byte_size belongs to this tick.
      std::vector<float> seen(std::size_t(buf.byte_size) / sizeof(float));
      if(!seen.empty())
        std::memcpy(seen.data(), buf.raw_data, std::size_t(buf.byte_size));
      REQUIRE(seen.size() == std::size_t(n) * 3);
      for(int i = 0; i < n; i++)
      {
        CHECK(seen[i * 3 + 0] == float(i));
        CHECK(seen[i * 3 + 1] == float(i) + 0.5f);
        CHECK(seen[i * 3 + 2] == float(-i));
      }

      // What a consumer is handed
      buf.upload((const char*)buf.raw_data, 0, buf.byte_size);
      buf.changed = false;
    }
  }

  REQUIRE(log.size() == 12);
  const std::vector<int64_t> expected_bytes{
      4 * 3 * 4, 10 * 3 * 4, 2 * 3 * 4, 7 * 3 * 4, 0, 3 * 3 * 4,
      4 * 3 * 4, 10 * 3 * 4, 2 * 3 * 4, 7 * 3 * 4, 0, 3 * 3 * 4};
  std::vector<int64_t> got;
  for(auto& r : log)
    got.push_back(r.bytesize);
  CHECK(got == expected_bytes);

  // The shrink from 10 points to 2: the consumer sees two points, and their
  // values are the new ones -- not the head of the ten-point cloud shifted.
  CHECK(log[2].floats.size() == 6);
  CHECK(log[2].floats[3] == 1.f);
  CHECK(log[2].floats[4] == 1.5f);
  CHECK(log[2].floats[5] == -1.f);
  // The empty cloud is an empty payload, not the previous one
  CHECK(log[4].floats.empty());
  // The second lifecycle is byte-identical to the first
  for(int i = 0; i < 6; i++)
    CHECK(log[i].floats == log[i + 6].floats);
}

TEST_CASE(
    "prepare() is re-run with the live buffer size and channel count",
    "[avnd][prepare]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = load_execution(*doc);

    auto& st = *plug.context().execState;
    const int engine_buffer = st.bufferSize;
    const double engine_rate = st.sampleRate;

    // The object is prepared for the engine's buffer size at construction
    // (setup_cpu ends with audio_configuration_changed). Start small so a
    // later, larger buffer size is representable.
    const int small = 128;
    st.bufferSize = small;

    mounted<PrepareProbe> m{*doc, plug, 8830};
    auto& obj = m.object();

    REQUIRE(!obj.setups.empty());
    CHECK(obj.setups.back().rate == engine_rate);
    CHECK(obj.setups.back().frames_per_buffer == small);
    CHECK(obj.host_rate == engine_rate);
    CHECK(obj.scratch.size() == std::size_t(small) * 4);

    const auto after_init = obj.setups.size();

    // A tick within the prepared buffer size: no re-preparation.
    m.tick(int(engine_rate), small);
    CHECK(obj.setups.size() == after_init);
    CHECK(*values_of(m.node(), 1)[0].target<int>() == small);

    // The audio engine's buffer size grew under a running object (a device
    // change): the token now carries more frames than the object was prepared
    // for, and avendish's prepare_run re-prepares.
    const int bigger = 512;
    st.bufferSize = bigger;
    m.tick(int(engine_rate), bigger);
    REQUIRE(obj.setups.size() == after_init + 1);
    CHECK(obj.setups.back().frames_per_buffer == bigger);
    CHECK(obj.setups.back().rate == engine_rate);
    // The object's own allocation followed the new size -- this is where a
    // prepare() sizes its accumulators.
    CHECK(obj.scratch.size() == std::size_t(bigger) * 4);
    CHECK(*values_of(m.node(), 1)[0].target<int>() == bigger);

    // Going back down does NOT re-prepare: the prepared buffer size is a
    // high-water mark, so the object keeps the larger allocation.
    m.tick(int(engine_rate), small);
    CHECK(obj.setups.size() == after_init + 1);
    CHECK(obj.scratch.size() == std::size_t(bigger) * 4);

    // A channel-count change on the audio inlet re-prepares with the new count
    {
      auto* in = m.node().root_inputs()[0]->target<ossia::audio_port>();
      REQUIRE(in);
      in->set_channels(5);
      for(auto& c : *in)
        c.resize(small);
    }
    m.tick(int(engine_rate), small);
    REQUIRE(obj.setups.size() == after_init + 2);
    CHECK(obj.setups.back().input_channels == 5);
    CHECK(obj.seen_channels == 5);

    // ... and again when it changes back
    {
      auto* in = m.node().root_inputs()[0]->target<ossia::audio_port>();
      in->set_channels(1);
      for(auto& c : *in)
        c.resize(small);
    }
    m.tick(int(engine_rate), small);
    REQUIRE(obj.setups.size() == after_init + 3);
    CHECK(obj.setups.back().input_channels == 1);
    CHECK(obj.seen_channels == 1);
    // The rate never changed under us: a sample-rate change rebuilds the node
    // instead (safe_node takes it in its constructor), which is the next case.
    for(auto& s : obj.setups)
      CHECK(s.rate == engine_rate);

    st.bufferSize = engine_buffer;
  });
}

TEST_CASE(
    "A rebuilt node prepares the object at the new sample rate", "[avnd][prepare]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = load_execution(*doc);

    auto& st = *plug.context().execState;
    const int engine_buffer = st.bufferSize;
    const double engine_rate = st.sampleRate;

    // safe_node takes the sample rate in its constructor and
    // audio_configuration_changed never changes it, so a rate change reaches
    // the object only through a rebuilt executor -- which is what an audio
    // device change does. An object that resamples against info.rate depends
    // on it, so the value it is prepared with is load-bearing.
    const double rates[] = {44100., 96000., 16000.};
    std::vector<double> prepared_with;
    std::vector<std::size_t> allocated;

    for(double rate : rates)
    {
      st.sampleRate = rate;
      st.bufferSize = 256;
      mounted<PrepareProbe> m{*doc, plug, 8840};
      auto& obj = m.object();
      REQUIRE(!obj.setups.empty());
      prepared_with.push_back(obj.setups.back().rate);
      allocated.push_back(obj.scratch.size());

      m.tick(int(rate), 256);
      // The object reports the rate it was prepared with, per tick
      CHECK(values_of(m.node(), 0)[0] == ossia::value{float(rate)});
      CHECK(obj.setups.back().frames_per_buffer == 256);
    }

    CHECK(prepared_with == std::vector<double>{44100., 96000., 16000.});
    CHECK(allocated == std::vector<std::size_t>{1024, 1024, 1024});

    st.sampleRate = engine_rate;
    st.bufferSize = engine_buffer;
  });
}

#if defined(SCORE_PLUGIN_GFX) && SCORE_PLUGIN_GFX
// 4b. The same resizing point cloud, observed where a score consumer observes
// it: NodeRenderer::bufferForOutput, through the real Crousti GPU path (any
// object with a buffer output is routed to setup_gpu, and its GfxRenderer calls
// buffer_outs.upload once per frame).
//
// The object also carries a texture output, for two reasons: a buffer output
// alone has no in-tree consumer to make the node reachable from a sink, and the
// texture readback proves the node really rendered the frames whose byte counts
// are being asserted.
namespace
{
std::atomic<int> g_gfx_instances{0};
std::atomic<int> g_gfx_live{0};
std::atomic<int> g_gfx_ticks{0};

struct PointCloudGfx
{
  halp_meta(name, "Test points gfx")
  halp_meta(c_name, "score_test_points_gfx")
  halp_meta(category, "Test")
  halp_meta(author, "score tests")
  halp_meta(description, "a point cloud on a cpu_buffer_output")
  halp_meta(uuid, "4e7c1b39-2a86-4f0d-b5c7-8d1e39a6f204")

  struct
  {
    halp::spinbox_i32<"Points", halp::range{0, 65536, 4}> points;
  } inputs;

  struct
  {
    halp::texture_output<"Out", halp::rgba_texture> image;
    halp::cpu_buffer_output<"Points"> points;
  } outputs;

  PointCloudGfx()
  {
    g_gfx_instances++;
    g_gfx_live++;
  }
  ~PointCloudGfx() { g_gfx_live--; }
  PointCloudGfx(const PointCloudGfx&) = delete;
  PointCloudGfx& operator=(const PointCloudGfx&) = delete;

  void operator()()
  {
    const int n = inputs.points.value;
    auto span = outputs.points.create<float>(int64_t(n) * 3);
    for(int i = 0; i < n; i++)
    {
      span[i * 3 + 0] = float(i);
      span[i * 3 + 1] = float(i) + 0.5f;
      span[i * 3 + 2] = float(-i);
    }
    outputs.points.upload();

    auto& out = outputs.image.texture;
    if(out.width != 8 || out.height != 8)
      outputs.image.create(8, 8);
    if(out.bytes)
    {
      for(int y = 0; y < 8; y++)
        for(int x = 0; x < 8; x++)
          outputs.image.set(x, y, 255, 255, 255, 255);
      outputs.image.upload();
    }
    g_gfx_ticks++;
  }
};

//! Deliver control values to a Crousti GPU node. CustomGfxNodeBase::process()
//! merges into last_message and keeps what it already had when a later message
//! carries none, so one call before rendering persists across the empty
//! per-frame Timings messages the pump sends.
void setInputs(score::gfx::Node& n, std::vector<ossia::value> vals)
{
  score::gfx::Message m;
  m.node_id = n.nodeId;
  for(auto& v : vals)
    m.input.push_back(std::move(v));
  n.process(std::move(m));
}

struct buffer_frame
{
  int requested{};
  int64_t view_byte_size{};
  int64_t handle_size{};
  const void* handle{};
  int instances{};
  int live{};
};
}

TEST_CASE(
    "A cpu_buffer_output resized every frame is handed to consumers at the right "
    "byte count, across renderer teardown",
    "[avnd][buffer][gfx]")
{
  using namespace score::test;
  using namespace score::test::gfx;

  // score::test::run_in_app forces QT_QPA_PLATFORM=offscreen for the headless
  // cases above, and that env var outlives the QApplication it was set for. Put
  // it back the way this process was started, or the RHI probe below would fail
  // for want of a platform plugin rather than for want of a GPU -- a skip that
  // would read as "no GPU" on a machine that has one.
  if(!g_qpa_platform_was_set)
    qunsetenv("QT_QPA_PLATFORM");

  const auto api = GENERATE(from_range(platform_backends()));

  IsfResult r;
  std::vector<buffer_frame> frames;
  std::vector<ReadbackImage> images;
  int instances_after_rebuild = 0;
  int renderers_when_unwired = -1;
  int live_when_unwired = -1;

  g_gfx_instances = 0;
  g_gfx_live = 0;
  g_gfx_ticks = 0;

  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* document = score::test::new_document(app);
    if(!document)
    {
      r.error = "could not create a document (ProcessModel needs one)";
      return;
    }
    const score::DocumentContext& ctx = document->context();

    std::vector<std::unique_ptr<Process::ProcessModel>> models;
    GfxPipeline p;

    auto model = std::make_unique<oscr::ProcessModel<PointCloudGfx>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));
    const int cloud = p.addNode(std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<PointCloudGfx>{*raw, {}, Gfx::exec_controls{}, 1, ctx}});

    auto* image_out = p.nodeImageOut(cloud, 0);
    auto* buffer_out = p.nodeBufferOut(cloud, 0);
    if(!image_out || !buffer_out)
    {
      r.error = "the object's texture / buffer outlets were not created";
      return;
    }

    const int sink = p.addSink({32, 32});
    p.wire(image_out, p.sinkInput(sink));

    if(!p.create(api))
    {
      r.backend = p.backend();
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();

    // What a downstream node is handed for this output, right now.
    auto observe = [&](int requested) {
      buffer_frame f;
      f.requested = requested;
      auto* n = p.node(cloud);
      if(n->renderedNodes.size() == 1)
      {
        auto view = n->renderedNodes.begin()->second->bufferForOutput(*buffer_out);
        f.view_byte_size = view.byte_size;
        f.handle = view.handle;
        f.handle_size = view.handle ? view.handle->size() : -1;
      }
      else
      {
        f.view_byte_size = -1;
        f.handle_size = -1;
      }
      f.instances = g_gfx_instances;
      f.live = g_gfx_live;
      frames.push_back(f);
    };

    // A cloud that grows, collapses, grows again, empties and comes back --
    // one resize per frame, which is what a sensor stream does.
    const std::vector<int> counts{4, 64, 2, 31, 0, 7};
    for(int n : counts)
    {
      setInputs(*p.node(cloud), {ossia::value{n}});
      p.render(1);
      observe(n);
    }
    images.push_back(p.readback(sink));

    // Resizing the sink rebuilds its RenderList target: the readback comes
    // back at the new size, and the byte counts must survive it.
    p.resizeSink(sink, {48, 48});
    p.render(1);

    // Genuine renderer teardown: unwiring the only edge makes the node
    // unreachable, and reconcileAllRenderLists releases its renderer, its
    // Node_T and its QRhiBuffer. Re-wiring builds a brand new one.
    p.removeEdgeIncremental(image_out, p.sinkInput(sink));
    renderers_when_unwired = int(p.node(cloud)->renderedNodes.size());
    live_when_unwired = g_gfx_live;
    p.render(1);

    p.addEdgeIncremental(image_out, p.sinkInput(sink));
    p.render(1);
    instances_after_rebuild = g_gfx_instances;

    for(int n : {9, 1, 40})
    {
      setInputs(*p.node(cloud), {ossia::value{n}});
      p.render(1);
      observe(n);
    }
    images.push_back(p.readback(sink));
  });

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());

  // The node really rendered: two valid readbacks at the two sink sizes.
  REQUIRE(images.size() == 2);
  REQUIRE(images[0].valid());
  REQUIRE(images[1].valid());
  CHECK(images[0].width == 32);
  CHECK(images[1].width == 48);
  CHECK(g_gfx_ticks >= 9);

  REQUIRE(frames.size() == 9);
  for(auto& f : frames)
  {
    INFO(
        "requested=" << f.requested << " view=" << f.view_byte_size
                     << " handle=" << f.handle_size);
    // Exactly this frame's payload, growing AND shrinking, before and after
    // the renderer was torn down and rebuilt.
    const int64_t expect = int64_t(f.requested) * 3 * int64_t(sizeof(float));
    if(f.requested > 0)
    {
      CHECK(f.view_byte_size == expect);
      CHECK(f.handle_size == expect);
    }
    else
    {
      // An empty cloud: score keeps a 1-byte placeholder buffer rather than a
      // null handle, and must not keep advertising the previous payload.
      CHECK(f.handle != nullptr);
      CHECK(f.view_byte_size <= 1);
      CHECK(f.handle_size <= 1);
    }
    // One object per renderer, and exactly one renderer here
    CHECK(f.live == 1);
  }

  // The teardown really happened: unwiring released the renderer and its
  // object, and re-wiring constructed a second one.
  CHECK(renderers_when_unwired == 0);
  CHECK(live_when_unwired == 0);
  CHECK(instances_after_rebuild == 2);
  CHECK(frames.back().instances == 2);
  CHECK(g_gfx_live == 0);
}

// 4c. The pure-producer shape: an object whose ONLY gfx port is a cpu buffer
// output. It never touches the RHI -- the renderers upload its bytes for it --
// but it has a CPU identity (such an object typically opens one network
// listener per instance). Two output windows are two RenderLists, and
// a RenderList's renderer is what builds the object: one object per window
// would be one listener per window on the same port, and tearing a renderer
// down and back up (a resize, an unwired output) would drop the connection.
//
// Crousti's CpuOnlyBufferNode is the classification, and GfxNode::rendererState
// is where it is honoured.
namespace
{
std::atomic<int> g_shared_instances{0};
std::atomic<int> g_shared_live{0};

struct PointCloudShared
{
  halp_meta(name, "Test points shared")
  halp_meta(c_name, "score_test_points_shared")
  halp_meta(category, "Test")
  halp_meta(author, "score tests")
  halp_meta(description, "a point cloud: a cpu_buffer_output and nothing else")
  halp_meta(uuid, "3b6d9e14-70c2-4a5f-9d83-51e7c206ab48")

  struct
  {
    halp::spinbox_i32<"Points", halp::range{0, 65536, 4}> points;
  } inputs;

  struct
  {
    halp::cpu_buffer_output<"Points"> points;
  } outputs;

  PointCloudShared()
  {
    g_shared_instances++;
    g_shared_live++;
  }
  ~PointCloudShared() { g_shared_live--; }
  PointCloudShared(const PointCloudShared&) = delete;
  PointCloudShared& operator=(const PointCloudShared&) = delete;

  void operator()()
  {
    const int n = inputs.points.value;
    auto span = outputs.points.create<float>(int64_t(n) * 3);
    for(int i = 0; i < n; i++)
    {
      span[i * 3 + 0] = float(i);
      span[i * 3 + 1] = float(i) + 0.5f;
      span[i * 3 + 2] = float(-i);
    }
    outputs.points.upload();
    ticks++;
  }

  int ticks{};
};

//! The consumer that makes the producer reachable from a sink: a buffer input
//! (what a point-cloud consumer is) plus a texture output to paint the sink
//! with.
struct PointCloudConsumer
{
  halp_meta(name, "Test points consumer")
  halp_meta(c_name, "score_test_points_consumer")
  halp_meta(category, "Test")
  halp_meta(author, "score tests")
  halp_meta(description, "buffer input -> texture output")
  halp_meta(uuid, "8c0a7f52-1d34-49be-a6f7-2b9e0d4c8351")

  struct
  {
    halp::cpu_buffer_input<"In"> points;
  } inputs;

  struct
  {
    halp::texture_output<"Out", halp::rgba_texture> image;
  } outputs;

  void operator()()
  {
    seen_bytes = inputs.points.buffer.byte_size;

    auto& out = outputs.image.texture;
    if(out.width != 8 || out.height != 8)
      outputs.image.create(8, 8);
    if(out.bytes)
    {
      for(int y = 0; y < 8; y++)
        for(int x = 0; x < 8; x++)
          outputs.image.set(x, y, 255, 255, 255, 255);
      outputs.image.upload();
    }
  }

  int64_t seen_bytes{-1};
};

score::gfx::Port* first_buffer_input(score::gfx::Node& n)
{
  for(auto* p : n.input)
    if(p->type == score::gfx::Types::Buffer)
      return p;
  return nullptr;
}
}

TEST_CASE(
    "A CPU-only buffer producer has one object for every RenderList",
    "[avnd][buffer][gfx]")
{
  using namespace score::test;
  using namespace score::test::gfx;

  if(!g_qpa_platform_was_set)
    qunsetenv("QT_QPA_PLATFORM");

  const auto api = GENERATE(from_range(platform_backends()));

  IsfResult r;
  int renderers = 0;
  int instances = 0;
  int live = 0;
  std::vector<int64_t> per_renderlist_bytes;
  int instances_after_rebuild = 0;
  int live_after_rebuild = 0;
  std::vector<int64_t> bytes_after_rebuild;
  std::vector<ReadbackImage> images;

  g_shared_instances = 0;
  g_shared_live = 0;

  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* document = score::test::new_document(app);
    if(!document)
    {
      r.error = "could not create a document (ProcessModel needs one)";
      return;
    }
    const score::DocumentContext& ctx = document->context();

    std::vector<std::unique_ptr<Process::ProcessModel>> models;
    GfxPipeline p;

    auto producer_model = std::make_unique<oscr::ProcessModel<PointCloudShared>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr);
    auto* producer_raw = producer_model.get();
    models.push_back(std::move(producer_model));
    const int cloud = p.addNode(std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<PointCloudShared>{*producer_raw, {}, Gfx::exec_controls{}, 1,
                                           ctx}});

    auto consumer_model = std::make_unique<oscr::ProcessModel<PointCloudConsumer>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{2}, ctx, nullptr);
    auto* consumer_raw = consumer_model.get();
    models.push_back(std::move(consumer_model));
    const int paint = p.addNode(std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<PointCloudConsumer>{*consumer_raw, {}, Gfx::exec_controls{}, 2,
                                             ctx}});

    auto* buffer_out = p.nodeBufferOut(cloud, 0);
    auto* buffer_in = first_buffer_input(*p.node(paint));
    auto* image_out = p.nodeImageOut(paint, 0);
    if(!buffer_out || !buffer_in || !image_out)
    {
      r.error = "the buffer / texture ports were not created";
      return;
    }

    // Two outputs: two RenderLists, each with its own RHI -- two windows.
    const int first = p.addSink({32, 32});
    const int second = p.addSink({32, 32});
    p.wire(buffer_out, buffer_in);
    p.wire(image_out, p.sinkInput(first));
    p.wire(image_out, p.sinkInput(second));

    if(!p.create(api))
    {
      r.backend = p.backend();
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();

    setInputs(*p.node(cloud), {ossia::value{7}});
    p.render(2);

    auto observe = [&](std::vector<int64_t>& out) {
      out.clear();
      for(auto& [rl, rn] : p.node(cloud)->renderedNodes)
        out.push_back(rn->bufferForOutput(*buffer_out).byte_size);
      std::sort(out.begin(), out.end());
    };

    renderers = int(p.node(cloud)->renderedNodes.size());
    instances = g_shared_instances;
    live = g_shared_live;
    observe(per_renderlist_bytes);
    images.push_back(p.readback(first));
    images.push_back(p.readback(second));

    // Tear one output's renderer down and bring it back: for a node whose
    // object is its CPU identity, that must not touch the object.
    p.removeEdgeIncremental(image_out, p.sinkInput(second));
    p.render(1);
    p.addEdgeIncremental(image_out, p.sinkInput(second));
    p.render(1);

    instances_after_rebuild = g_shared_instances;
    live_after_rebuild = g_shared_live;
    observe(bytes_after_rebuild);
  });

  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error: " << r.error);
  REQUIRE(r.error.empty());

  // Both windows rendered.
  REQUIRE(images.size() == 2);
  CHECK(images[0].valid());
  CHECK(images[1].valid());

  // Two RenderLists, so two renderers on the producer...
  CHECK(renderers == 2);
  // ... and exactly one object behind them: the CPU identity is not
  // duplicated per output window.
  CHECK(instances == 1);
  CHECK(live == 1);

  // Each renderer still publishes the cloud into its own buffer.
  REQUIRE(per_renderlist_bytes.size() == 2);
  for(auto b : per_renderlist_bytes)
    CHECK(b == 7 * 3 * int64_t(sizeof(float)));

  // A renderer torn down and rebuilt reuses the same object -- its network
  // listener stays up -- and the byte counts are still exact afterwards.
  CHECK(instances_after_rebuild == 1);
  CHECK(live_after_rebuild == 1);
  REQUIRE(bytes_after_rebuild.size() == 2);
  for(auto b : bytes_after_rebuild)
    CHECK(b == 7 * 3 * int64_t(sizeof(float)));
}
#endif
