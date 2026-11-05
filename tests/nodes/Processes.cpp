// L1 test harness — generic process sweep.
//
// Iterates the entire Process::ProcessFactoryList exposed by the running
// (headless) application and, for EVERY registered process/node type:
//
//   1. constructs the model through the factory (the same construction path
//      AddOnlyProcessToInterval uses: factory->make(duration, data, id, ctx,
//      parent), with data = factory->customConstructionData(), or the
//      validation plug-in a host of audio plug-ins is made from), inside a real
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
// The denylist (SweepGuard.hpp) is ONLY for factories whose construction cannot be guarded
// at all with no plug-in to host: LV2 blocks forever scanning the system plugin
// dirs, VST/VST3 spawn an off-main-thread module scan that segfaults where
// siglongjmp cannot recover, and CLAP traps. Each of them is swept once for every
// validation plug-in of its format instead (ValidationPlugins.hpp), and skipped
// only when none is installed. Every other host/script node (Faust, Javascript,
// PureData, ...) is swept and reported honestly.

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

#include "SweepGuard.hpp"
#include "ValidationPlugins.hpp"

#include <set>
#include <string>
#include <vector>

namespace
{
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

  //! It hosts a validation plug-in, and the ports the plug-in gave it
  bool plugin = false;
  std::size_t ports = 0;
};

// Structural equality used by both round-trip paths.
bool same_shape(const Process::ProcessModel& a, const Process::ProcessModel& b);

// Full L1 work for a single factory: construct from `data`, DataStream
// round-trip, JSON round-trip. Runs inside the crash guard so a hard-crashing
// constructor is recorded rather than aborting the sweep.
void sweep_one(
    Process::ProcessModelFactory& factory, const QString& data,
    const Process::ProcessFactoryList& pl, const score::DocumentContext& docctx,
    QObject* parent, const TimeVal& duration, Id<Process::ProcessModel> id,
    SweepResult& r)
{
  const bool trace = qEnvironmentVariableIsSet("SWEEP_TRACE");
  if(trace)
    qWarning().noquote() << "SWEEP make" << QString::fromStdString(r.uuid)
                         << QString::fromStdString(r.name);

  Process::ProcessModel* proc = nullptr;
  try
  {
    proc = factory.make(duration, data, id, docctx, parent);
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
  r.ports = proc->inlets().size() + proc->outlets().size();

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

  // SWEEP_ONLY restricts the sweep to some factories (SweepGuard.hpp)
  const std::set<std::string> only = sweep::only();

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

    // The hosts of audio plug-ins are made once for each validation plug-in
    const auto plugins = sweep::ValidationPlugins::make(
        ctx, pl, [&](const std::string& uuid) { return only.empty() || only.contains(uuid); });

    for(auto [factory_ptr, plugin] : sweep::jobs(pl, plugins.get()))
    {
      auto& factory = *factory_ptr;
      results.emplace_back();
      const std::size_t idx = results.size() - 1;
      results[idx].uuid
          = score::uuids::toByteArray(factory.concreteKey().impl()).toStdString();
      results[idx].name = factory.prettyName().toStdString();
      if(plugin)
      {
        results[idx].name += " (" + plugin->label() + ")";
        results[idx].plugin = true;
      }

      if(!only.empty() && !only.contains(results[idx].uuid))
      {
        results[idx].skipped = true;
        results[idx].skip_reason = "not selected (SWEEP_ONLY)";
        continue;
      }

      if(plugin && !plugin->skip.empty())
      {
        results[idx].skipped = true;
        results[idx].skip_reason = plugin->skip;
        continue;
      }

      if(!plugin && sweep::construction_denylist.contains(results[idx].uuid))
      {
        results[idx].skipped = true;
        results[idx].skip_reason
            = "construction denylist (headless-unsafe external resource)";
        continue;
      }

      const QString data = plugin ? plugin->data : factory.customConstructionData();
      if(!sweep::guarded(
             [&] {
        sweep_one(
            factory, data, pl, doc->context(), sweepParent, duration,
            Id<Process::ProcessModel>{id_counter++}, results[idx]);
      }, results[idx].detail))
        results[idx].crashed = true;
    }

    // Detach from the document and intentionally leak: half-constructed models
    // left by an in-guard crash must never be destroyed at document teardown.
    sweepParent->setParent(nullptr);
  };

  // A construction or round-trip crash is recorded per factory and the sweep
  // goes on; a crash at document or application teardown, once every result is
  // collected, is escaped.
  if(!sweep::run_with_teardown_guard([&] { score::test::run_in_app(sweep); }))
    WARN(
        "Document/app teardown crashed (fatal signal "
        << (int)sweep::last_signal
        << ") after the sweep completed; results were collected before teardown.");

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
    // A host that loaded its plug-in has the ports of the plug-in
    if(r.plugin && r.constructed)
      CHECK(r.ports > 0);

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
