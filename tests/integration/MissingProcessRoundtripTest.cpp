// =============================================================================
// EXPECTED RED ON THIS BRANCH, ON PURPOSE. Read this before "fixing" it.
//
// This test asserts a CONTRACT, and it is deliberately implementation-agnostic:
// it names no placeholder class and no factory internals, only what a user can
// observe after a round trip -- the process is still there, under its original
// uuid, with its ports, and the cable into it still resolves.
//
// The implementation it was written alongside (Process::MissingProcess,
// f2b5a07da2) was REVERTED: the user decided PR #2179 "Survive a build that
// does not have every plug-in" owns this problem, and it solves it far more
// broadly -- ports, layers, document plug-ins, device protocols, network peers
// and curve segments, where ours covered the process and its cables. Keeping
// both would have conflicted on merge.
//
// The test is kept rather than reverted with it, because #2179's own pin
// (a60e6e871b, tests/unit/HeterogeneousBuildTest.cpp "A process with no factory
// keeps its identity and data") is a UNIT test over deserialize_interface: it
// checks the uuid, the port COUNTS and a byte-identical payload round trip, all
// of which are stronger than ours in their own way -- but it never builds a
// document, never attaches a cable, and so cannot see a cable dropped because
// its endpoint vanished. That was the actual reported symptom of A16. An
// independent check written against someone else's implementation is worth more
// than one written alongside it.
//
// So: this goes GREEN when #2179 lands, and the [!shouldfail] tag comes off in
// the same commit. Until then loadMissing is back to `SCORE_TODO; return
// nullptr;` and it fails by DETECTING THE LOSS rather than by crashing --
// `CHECK(int(interval.processes.size()) == process_count)` reads 2 == 3.
// =============================================================================
// A16 — a document that names a process this build does not have must come back
// WITH that process, its ports and its cables.
//
// THE DEFECT. `Process::ProcessFactoryList::loadMissing` was
//     SCORE_TODO; return nullptr;
// and its caller (IntervalModelSerialization, both the DataStream and the JSON
// path) was `if(proc) ... else SCORE_TODO;`. So a document saved with an add-on
// the current build does not have came back with the process GONE — and, since
// its ports went with it, every cable attached to it gone too — while the
// document still reported as having loaded successfully. The next save made the
// loss permanent. It is the only silent data-loss path in the loader: every
// other failure is loud (a crash, a misrender, an abort).
//
// Concretely: a user without score-addon-academy loses the `OpenPBR LUTs`
// process out of 9 documents, and 7 documents in the 260-document corpus are
// damaged on load for the same reason.
//
// WHY THIS TEST IS SHAPED LIKE THIS. Asserting "the document opened" is exactly
// the assertion that hides this bug — a damaged document opens perfectly well.
// So the test asserts on the CONTENT: after the round trip the process is still
// there, under its original uuid, with the ports it had, and the cable into it
// is still attached. It also re-saves and checks that the unknown uuid is
// written back, because a placeholder that loses the identity on save is just a
// slower version of the same data loss.
//
// HOW THE UNKNOWN UUID IS PRODUCED. Rather than ship a fixture document that
// would rot, the test builds a real two-process document with a cable, saves it
// as JSON (the `.score` format), and rewrites ONE process's uuid to a uuid no
// factory claims. That is byte-for-byte the situation of a document written by
// a build with an extra plug-in.
//
//   ctest -R integration_missing_process_roundtrip --output-on-failure
// =============================================================================
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Dataflow/Commands/EditConnection.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <catch2/catch_test_macros.hpp>

#include <set>

namespace
{
// Automation: no inlet, one Message outlet.
const QString automation_uuid = QStringLiteral("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f");
// Mapping: one Message inlet, one Message outlet. This is the one whose uuid the
// test rewrites, so BOTH directions of port restoration are exercised.
const QString mapping_uuid = QStringLiteral("12a5d9b8-823e-4303-99f8-34db37c448b4");
// Claimed by no factory, in this build or any other.
const QString unknown_uuid = QStringLiteral("00000000-dead-4bee-8000-000000000a16");

//! Keys that Entity<ProcessModel> / ProcessModel write themselves. Everything
//! else in a process object is the concrete process's own payload, which a
//! placeholder must hand back untouched.
const std::set<QString> base_keys{
    "uuid", "id",   "ObjectName",   "Metadata", "Components",   "Duration",
    "Height", "StartOffset", "LoopDuration", "Pos", "Size", "Loops", "FoldMode"};

//! Depth-first search for the process object carrying `uuid`.
std::optional<QJsonObject> find_process(const QJsonValue& v, const QString& uuid)
{
  if(v.isObject())
  {
    const auto o = v.toObject();
    if(o.value("uuid").toString() == uuid && o.contains("Duration"))
      return o;
    for(const auto& k : o.keys())
      if(auto r = find_process(o.value(k), uuid))
        return r;
  }
  else if(v.isArray())
  {
    for(const auto& e : v.toArray())
      if(auto r = find_process(e, uuid))
        return r;
  }
  return std::nullopt;
}

Scenario::ScenarioDocumentModel& doc_model(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate());
}

QByteArray save_json(score::Document& doc)
{
  JSONReader w;
  doc.saveAsJson(w);
  return w.toByteArray();
}
} // namespace

TEST_CASE(
    "a process whose factory is missing keeps its identity, its ports and its "
    "cables across a load",
    "[integration][serialization][missing][!shouldfail]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
    REQUIRE(!delegates.empty());

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* source = score::test::add_process(*doc, automation_uuid, {});
    auto* sink = score::test::add_process(*doc, mapping_uuid, {});
    if(!source || !sink)
      SKIP("this build has neither Automation nor Mapping, so there is nothing "
           "to wire");

    REQUIRE(!source->outlets().empty());
    REQUIRE(!sink->inlets().empty());
    const int sink_inlets = int(sink->inlets().size());
    const int sink_outlets = int(sink->outlets().size());

    // The base interval already carries the document's root Scenario process.
    const int process_count = int(score::test::base_interval(*doc).processes.size());

    auto& dp = doc_model(*doc);
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Dataflow::CreateCable{
            dp, Id<Process::Cable>{4242}, Process::CableType::ImmediateGlutton,
            *source->outlets()[0], *sink->inlets()[0]});
    QApplication::processEvents();
    REQUIRE(dp.cables.size() == 1);

    // Save, then pretend the Mapping came from a plug-in we do not have.
    QByteArray json = save_json(*doc);
    REQUIRE(json.contains(mapping_uuid.toUtf8()));
    json.replace(mapping_uuid.toUtf8(), unknown_uuid.toUtf8());
    REQUIRE(!json.contains(mapping_uuid.toUtf8()));

    score::Document* reloaded = ctx.docManager.loadDocument(
        ctx, QStringLiteral("missing-process"), json, JSONObject::type(),
        *delegates.begin());
    REQUIRE(reloaded != nullptr);
    QApplication::processEvents();

    // 1. The process is still in the interval.
    auto& interval = score::test::base_interval(*reloaded);
    CHECK(int(interval.processes.size()) == process_count);

    Process::ProcessModel* kept{};
    for(auto& p : interval.processes)
      if(p.concreteKey()
         == UuidKey<Process::ProcessModel>::fromString(unknown_uuid))
        kept = &p;
    REQUIRE(kept != nullptr);

    // 2. ...under its ORIGINAL identity, so a build that has the plug-in loads
    //    it normally.
    CHECK(
        kept->concreteKey()
        == UuidKey<Process::ProcessModel>::fromString(unknown_uuid));

    // 3. ...with the ports it was saved with. This is what the cables hang off.
    CHECK(int(kept->inlets().size()) == sink_inlets);
    CHECK(int(kept->outlets().size()) == sink_outlets);

    // 4. ...and the cable into it survived.
    CHECK(doc_model(*reloaded).cables.size() == 1);

    // 5. Saving again writes the unknown process back, so opening the document
    //    on a machine that HAS the plug-in gets it intact.
    const QByteArray resaved = save_json(*reloaded);
    CHECK(resaved.contains(unknown_uuid.toUtf8()));

    // 6. ...and it writes back the process's OWN payload byte-for-byte, not a
    //    hollow stand-in. A placeholder that keeps the uuid but loses the
    //    Curve, the ranges and the port definitions is just a slower data loss:
    //    the build that HAS the plug-in would reopen an empty process.
    const auto before
        = find_process(QJsonDocument::fromJson(json).object(), unknown_uuid);
    const auto after
        = find_process(QJsonDocument::fromJson(resaved).object(), unknown_uuid);
    REQUIRE(before.has_value());
    REQUIRE(after.has_value());

    for(const auto& k : before->keys())
    {
      if(base_keys.count(k))
        continue;
      INFO("payload key: " << k.toStdString());
      CHECK(after->contains(k));
      CHECK(after->value(k) == before->value(k));
    }
    // No keys invented either.
    for(const auto& k : after->keys())
    {
      if(base_keys.count(k))
        continue;
      INFO("unexpected key in the re-saved process: " << k.toStdString());
      CHECK(before->contains(k));
    }
  });
}
