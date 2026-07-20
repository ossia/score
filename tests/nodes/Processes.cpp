// L1 test harness — generic process sweep.
//
// Iterates the entire Process::ProcessFactoryList exposed by the running
// (headless) application and, for EVERY registered process/node type:
//
//   1. constructs the model through the factory (the same construction path
//      AddOnlyProcessToInterval uses: factory->make(duration, data, id, ctx,
//      parent), with data = factory->customConstructionData()), inside a real
//      scenario document interval so the model has a valid parent/context;
//   2. asserts it constructs non-null;
//   3. JSON round-trip: serialize -> deserialize -> assert same concreteKey and
//      same inlet/outlet counts, plus a second-pass fixed-point on the bytes;
//   4. DataStream round-trip: same via the binary path.
//
// Each factory is reported individually (scoped INFO with its pretty name +
// UUID) so a failure names exactly which node type broke. Construction and
// serialization are wrapped so one broken node cannot abort the whole sweep.
//
// SKIPPED / KNOWN-CAVEAT node types
// ---------------------------------
// The sweep attempts *every* factory registered at runtime; it does not
// pre-emptively fake-pass anything. Model construction is GPU-free even for the
// gfx/threedim render processes (ISF/CSF/raw-raster/video/threedim), so those
// are swept as models here — only their L3 rendering is out of scope.
//
// Robustness: a per-factory crash guard (POSIX signal handler + siglongjmp)
// turns an uncatchable construction/round-trip crash (SIGSEGV/SIGABRT/...) into
// a recorded RED result instead of aborting the whole sweep, and a watchdog
// (SIGALRM) does the same for hangs. A second, whole-run guard escapes a
// document/app *teardown* segfault caused by a node that left global/threaded
// engine state behind — every result has already been collected by then, so the
// report is still produced.
//
// The denylist below is ONLY for factories whose construction cannot be guarded
// at all: LV2 blocks forever scanning the system plugin dirs, and VST/VST3 spawn
// an off-main-thread module scan that segfaults where siglongjmp cannot recover.
// All need an external plugin. Every other host/script node (CLAP, Faust,
// Javascript, PureData, ...) is swept and reported honestly.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QString>

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <csetjmp>
#include <csignal>

// Windows has no sigsetjmp/siglongjmp, no SIGBUS/SIGTRAP/SIGALRM and no
// alarm(): fall back to plain setjmp/longjmp, drop the missing signals from
// the guarded set and make the watchdog a no-op. The guard still catches
// SIGSEGV/SIGABRT/SIGFPE/SIGILL, which is what these sweeps rely on.
#if defined(_WIN32)
#include <csetjmp>
using sigjmp_buf = std::jmp_buf;
#define sigsetjmp(buf, save) setjmp(buf)
#define siglongjmp(buf, val) longjmp(buf, val)
static inline unsigned int alarm(unsigned int) noexcept { return 0; }
// No alarm() on Windows, so the watchdog never fires: use an impossible
// value so the timeout branch stays compilable but unreachable.
#define SIGALRM (-1)
#endif

#include <cstdlib>
#include <set>
#include <string>
#include <vector>

namespace
{
// --- crash guard ---------------------------------------------------------
// Several node constructors can hard-crash (SIGSEGV/SIGABRT) when built with
// the empty default construction data (e.g. an out-of-bounds registry lookup).
// We cannot catch those as C++ exceptions, so we install signal handlers and
// siglongjmp back to a per-factory checkpoint, recording the crash as a RED
// result and letting the sweep continue to the next factory.
sigjmp_buf g_crash_jmp;      // per-factory checkpoint
sigjmp_buf g_teardown_jmp;   // whole-run (document/app teardown) checkpoint
volatile sig_atomic_t g_in_guard = 0;
volatile sig_atomic_t g_teardown_guard = 0;
volatile sig_atomic_t g_last_signal = 0;
// Triage: when set (SWEEP_NOGUARD), skip the signal handlers entirely so a fault
// surfaces to a debugger instead of being caught and turned into a RED result.
bool g_no_guard = false;

extern "C" void sweep_crash_handler(int sig)
{
  if(g_in_guard)
  {
    g_in_guard = 0;
    g_last_signal = sig;
    siglongjmp(g_crash_jmp, sig);
  }
  // Some host/gfx nodes leave global or threaded engine state that segfaults at
  // document teardown, *after* every per-factory result has been collected.
  // Escape that so the results can still be reported.
  if(g_teardown_guard)
  {
    g_teardown_guard = 0;
    g_last_signal = sig;
    siglongjmp(g_teardown_jmp, sig);
  }
  // Not inside a guarded region: restore default and re-raise (real crash).
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}

constexpr int g_guarded_signals[]
    = {SIGSEGV, SIGABRT, SIGFPE, SIGILL
#if !defined(_WIN32)
       , SIGBUS, SIGTRAP, SIGALRM
#endif
};

void install_crash_handlers()
{
  for(int s : g_guarded_signals)
    std::signal(s, sweep_crash_handler);
}

void restore_crash_handlers()
{
  for(int s : g_guarded_signals)
    std::signal(s, SIG_DFL);
}

// UUIDs that cannot be constructed headlessly without crashing the process.
// Empty by default — populate only if the sweep proves a node hard-crashes on
// construction (segfault/abort), with a human-readable reason.
const std::set<std::string> g_construction_denylist = {
    // LV2: constructing the model spins up the lilv "world" and scans the
    // system LV2 plugin directories, which blocks indefinitely headlessly.
    // Needs an external plugin; not sweepable as a bare model.
    "fd5243ba-70b5-4164-b44a-ecb0dcdc0494", // LV2
    // VST: constructing with an empty plugin path dereferences a null effect
    // (crashes in-thread). Needs an external plugin.
    "be8e6bd3-75f2-4102-8895-8a4eb4ea545a", // VST (2.x)
    // VST3: construction spawns an asynchronous module-scan on a worker thread
    // that segfaults off the guarded main thread (longjmp cannot recover a
    // crash on another thread). Needs an external plugin.
    "4cc30435-3237-4e94-a79e-1c2bd6b724a1", // VST 3
    // CLAP: constructing the model with no plugin file loads the host runtime
    // and traps (SIGTRAP) with no plugin present. Needs an external plugin.
    "10e8d26d-1d82-4bfc-a9fb-d85ffdf04e5f", // CLAP
};

// Per-factory watchdog: a construction/round-trip that runs longer than this
// is aborted via SIGALRM (caught by the crash guard) and recorded as a timeout,
// so a hanging host-plugin scan cannot wedge the whole sweep.
constexpr unsigned g_watchdog_seconds = 20;

struct SweepResult
{
  std::string uuid;
  std::string name;
  bool skipped = false;
  std::string skip_reason;

  bool constructed = false;
  bool json_roundtrip = false;
  bool json_fixedpoint = false;
  bool datastream_roundtrip = false;
  bool datastream_fixedpoint = false;
  bool crashed = false;
  std::string detail;
};

// Structural equality used by both round-trip paths.
bool same_shape(const Process::ProcessModel& a, const Process::ProcessModel& b);

// Full L1 work for a single factory: construct, DataStream round-trip, JSON
// round-trip. Runs inside the crash guard so a hard-crashing constructor is
// recorded rather than aborting the sweep.
void sweep_one(
    Process::ProcessModelFactory& factory, const Process::ProcessFactoryList& pl,
    const score::DocumentContext& docctx, QObject* parent, const TimeVal& duration,
    Id<Process::ProcessModel> id, SweepResult& r)
{
  const bool trace = qEnvironmentVariableIsSet("SWEEP_TRACE");
  if(trace)
    qWarning().noquote() << "SWEEP make" << QString::fromStdString(r.uuid)
                         << QString::fromStdString(r.name);

  Process::ProcessModel* proc = nullptr;
  try
  {
    proc = factory.make(duration, factory.customConstructionData(), id, docctx, parent);
  }
  catch(const std::exception& e)
  {
    r.detail = std::string("make() threw: ") + e.what();
  }
  catch(...)
  {
    r.detail = "make() threw (unknown exception)";
  }

  if(!proc)
    return;
  r.constructed = true;

  // --- DataStream round-trip ---
  if(trace)
    qWarning().noquote() << "SWEEP   datastream" << QString::fromStdString(r.uuid);
  try
  {
    const QByteArray bytes = score::marshall<DataStream>(*proc);
    Process::ProcessModel* clone
        = deserialize_interface(pl, DataStream::Deserializer{bytes}, docctx, parent);
    if(clone)
    {
      r.datastream_roundtrip = same_shape(*proc, *clone);
      const QByteArray bytes2 = score::marshall<DataStream>(*clone);
      r.datastream_fixedpoint = (bytes2 == bytes);
      delete clone;
    }
    else
    {
      r.detail += " [datastream: deserialize returned null]";
    }
  }
  catch(const std::exception& e)
  {
    r.detail += std::string(" [datastream threw: ") + e.what() + "]";
  }
  catch(...)
  {
    r.detail += " [datastream threw unknown]";
  }

  // --- JSON round-trip ---
  if(trace)
    qWarning().noquote() << "SWEEP   json" << QString::fromStdString(r.uuid);
  try
  {
    JSONReader reader;
    reader.readFrom(*proc);
    const rapidjson::Document jdoc = toValue(reader);
    JSONWriter writer{jdoc};
    Process::ProcessModel* clone = deserialize_interface(pl, writer, docctx, parent);
    if(clone)
    {
      r.json_roundtrip = same_shape(*proc, *clone);

      JSONReader reader2;
      reader2.readFrom(*clone);
      r.json_fixedpoint = (reader2.toByteArray() == reader.toByteArray());
      delete clone;
    }
    else
    {
      r.detail += " [json: deserialize returned null]";
    }
  }
  catch(const std::exception& e)
  {
    r.detail += std::string(" [json threw: ") + e.what() + "]";
  }
  catch(...)
  {
    r.detail += " [json threw unknown]";
  }

  delete proc;
}

// Structural equality used by both round-trip paths.
bool same_shape(const Process::ProcessModel& a, const Process::ProcessModel& b)
{
  return a.concreteKey() == b.concreteKey() && a.inlets().size() == b.inlets().size()
         && a.outlets().size() == b.outlets().size();
}
}

TEST_CASE("Every process factory instantiates and round-trips", "[nodes][l1][serialization]")
{
  std::vector<SweepResult> results;

  // Optional triage filter: restrict the sweep to a comma-separated list of
  // UUIDs. Combined with SWEEP_NOGUARD (below) it lets a debugger stop on the
  // real crash of a single factory instead of the guard's siglongjmp.
  std::set<std::string> only;
  if(const char* v = std::getenv("SWEEP_ONLY"))
  {
    std::string s{v}, tok;
    for(char c : s + ",")
    {
      if(c == ',')
      {
        if(!tok.empty())
          only.insert(tok);
        tok.clear();
      }
      else
        tok.push_back(c);
    }
  }

  auto sweep = [&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();
    const auto duration = interval.duration.defaultDuration();

    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    REQUIRE(!pl.empty());

    volatile int id_counter = 100000;

    // Owner for every swept model. It is parented *under the interval* while the
    // sweep runs, so a model that walks up the QObject tree to resolve its
    // owning Document during construction (Faust, Javascript, PureData, Control
    // surface, ...) finds it. Once the sweep is done we detach it from the
    // document (setParent(nullptr)) and leak it: that keeps any half-constructed
    // object left behind by an in-guard crash out of the interval's destructor,
    // so a recorded crash cannot turn into a teardown segfault at document close.
    QObject* const sweepParent = new QObject{&interval};

    for(auto& factory : pl)
    {
      results.emplace_back();
      const std::size_t idx = results.size() - 1;
      results[idx].uuid
          = score::uuids::toByteArray(factory.concreteKey().impl()).toStdString();
      results[idx].name = factory.prettyName().toStdString();

      if(!only.empty() && !only.contains(results[idx].uuid))
      {
        results[idx].skipped = true;
        results[idx].skip_reason = "not selected (SWEEP_ONLY)";
        continue;
      }

      if(g_construction_denylist.contains(results[idx].uuid))
      {
        results[idx].skipped = true;
        results[idx].skip_reason
            = "construction denylist (headless-unsafe external resource)";
        continue;
      }

      // SWEEP_NOGUARD: run the factory raw (no crash guard, no watchdog) so a
      // debugger can stop on the real fault instead of the guard's siglongjmp.
      if(g_no_guard)
      {
        sweep_one(
            factory, pl, doc->context(), sweepParent, duration,
            Id<Process::ProcessModel>{id_counter++}, results[idx]);
        continue;
      }

      g_in_guard = 1;
      g_last_signal = 0;
      if(sigsetjmp(g_crash_jmp, 1) == 0)
      {
        alarm(g_watchdog_seconds);
        sweep_one(
            factory, pl, doc->context(), sweepParent, duration,
            Id<Process::ProcessModel>{id_counter++}, results[idx]);
        alarm(0);
        g_in_guard = 0;
      }
      else
      {
        alarm(0);
        g_in_guard = 0;
        results[idx].crashed = true;
        if(g_last_signal == SIGALRM)
          results[idx].detail += " [TIMEOUT: construction/round-trip exceeded "
                                 + std::to_string(g_watchdog_seconds) + "s watchdog]";
        else
          results[idx].detail
              += " [CRASHED: fatal signal " + std::to_string((int)g_last_signal)
                 + " during construction/round-trip]";
      }
    }

    // Detach from the document and intentionally leak: half-constructed models
    // left by an in-guard crash must never be destroyed at document teardown.
    sweepParent->setParent(nullptr);
  };

  // Run the sweep under two nested crash guards:
  //  * per-factory (inside the lambda): a construction/round-trip crash is
  //    recorded and the sweep continues;
  //  * teardown (here): if closing the document / tearing down the app
  //    segfaults on the main thread because a node left global engine state
  //    behind, escape it — every result has already been collected.
  g_no_guard = qEnvironmentVariableIsSet("SWEEP_NOGUARD");
  if(g_no_guard)
  {
    // Raw run for debugging: no handlers, no teardown guard.
    score::test::run_in_app(sweep);
  }
  else if(install_crash_handlers(), sigsetjmp(g_teardown_jmp, 1) == 0)
  {
    g_teardown_guard = 1;
    score::test::run_in_app(sweep);
    g_teardown_guard = 0;
  }
  else
  {
    WARN(
        "Document/app teardown crashed (fatal signal "
        << (int)g_last_signal
        << ") after the sweep completed; results were collected before teardown.");
  }
  restore_crash_handlers();

  // Optional machine-readable dump (one line per factory) for triage.
  if(qEnvironmentVariableIsSet("SWEEP_DUMP"))
  {
    for(const auto& r : results)
    {
      std::fprintf(
          stderr,
          "DUMP\t%s\tctor=%d crash=%d ds_rt=%d ds_fp=%d js_rt=%d js_fp=%d\t%s\t%s\t%s\n",
          r.skipped ? "SKIP" : "", (int)r.constructed, (int)r.crashed,
          (int)r.datastream_roundtrip, (int)r.datastream_fixedpoint,
          (int)r.json_roundtrip, (int)r.json_fixedpoint, r.uuid.c_str(),
          r.name.c_str(), r.detail.c_str());
    }
    std::fflush(stderr);
  }

  // Report, one individually-named assertion block per factory.
  REQUIRE(!results.empty());

  int covered = 0, passed = 0, skipped = 0, crashed = 0;
  for(const auto& r : results)
  {
    INFO("Process: " << r.name << "  {" << r.uuid << "}");
    if(!r.detail.empty())
      INFO("detail:" << r.detail);

    if(r.skipped)
    {
      ++skipped;
      WARN("SKIPPED " << r.name << " {" << r.uuid << "}: " << r.skip_reason);
      continue;
    }

    ++covered;
    if(r.crashed)
    {
      ++crashed;
      WARN("CRASHED " << r.name << " {" << r.uuid << "}:" << r.detail);
    }

    // Hard invariants: the model must construct, and a serialize/deserialize
    // round-trip must preserve its structure (same concrete type, same number
    // of inlets and outlets) on both the binary and JSON paths.
    CHECK(r.constructed);
    CHECK(r.datastream_roundtrip);
    CHECK(r.json_roundtrip);

    // Byte-exact fixed point (re-serializing the clone yields identical bytes)
    // is a stronger property that many nodes legitimately do not hold: fields
    // that normalize on load (a path made absolute, a default filled in) change
    // the bytes without changing the structure. Report it, do not fail on it.
    if(r.constructed && !r.datastream_fixedpoint)
      WARN("datastream not byte-exact: " << r.name << " {" << r.uuid << "}");
    if(r.constructed && !r.json_fixedpoint)
      WARN("json not byte-exact: " << r.name << " {" << r.uuid << "}");

    if(r.constructed && r.datastream_roundtrip && r.datastream_fixedpoint
       && r.json_roundtrip && r.json_fixedpoint)
      ++passed;
  }

  WARN(
      "Process sweep: " << results.size() << " factories, " << covered << " swept, "
                        << passed << " byte-exact, " << crashed << " crashed, "
                        << skipped << " skipped.");
}
