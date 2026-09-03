// Integration test: a scenario document with actual content (a process on the
// base interval) must round-trip through serialization unchanged, and the
// reloaded document must contain the same content.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QPointF>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
Scenario::IntervalModel& base_interval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}

std::vector<int32_t> process_ids(const Scenario::IntervalModel& itv)
{
  std::vector<int32_t> ids;
  for(const auto& p : itv.processes)
    ids.push_back(p.id().val());
  return ids;
}

//! Add `n` copies of a known process factory to `itv`, through the same command
//! the editor uses.
void add_processes(score::Document& doc, Scenario::IntervalModel& itv, int n)
{
  auto& factories = doc.context().app.interfaces<Process::ProcessFactoryList>();
  auto* factory = factories.get(UuidKey<Process::ProcessModel>::fromString(
      QStringLiteral("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f")));
  REQUIRE(factory != nullptr);

  CommandDispatcher<> disp{doc.context().commandStack};
  for(int i = 0; i < n; i++)
    disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
        itv, factory->concreteKey(), factory->customConstructionData(), QPointF{});
}
}

TEST_CASE("A scenario with a process round-trips", "[integration][serialization]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval = base_interval(*doc);

    auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
    auto* factory = factories.get(UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f")));
    REQUIRE(factory != nullptr);

    const auto initial = interval.processes.size();

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
        interval, factory->concreteKey(), factory->customConstructionData(), QPointF{});
    REQUIRE(interval.processes.size() == initial + 1);

    score::Document* reloaded = score::test::reload_via_bytes(ctx, *doc);
    REQUIRE(reloaded != nullptr);

    // The reloaded document preserves the process.
    CHECK(base_interval(*reloaded).processes.size() == initial + 1);
  });
}

// Defect: DocumentRoundtripTest proves save -> load -> save is a byte fixed
// point for a fresh document — which already holds the base Scenario process,
// so "has content" is not what breaks stability. Adding one more process is:
// the re-save of the reloaded document differs from the original save. When
// serialization becomes stable for added processes this passes and the
// [!shouldfail] tag comes off.
TEST_CASE(
    "A scenario with an added process stays a byte fixed point",
    "[integration][serialization][!shouldfail]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval = base_interval(*doc);
    auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
    auto* factory = factories.get(UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f")));
    REQUIRE(factory != nullptr);

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
        interval, factory->concreteKey(), factory->customConstructionData(), QPointF{});

    const QByteArray original = doc->saveAsByteArray();
    score::Document* reloaded = score::test::reload_via_bytes(ctx, *doc);
    REQUIRE(reloaded != nullptr);

    const QByteArray resaved = reloaded->saveAsByteArray();
    std::size_t firstDiff = 0;
    const auto n = std::min(original.size(), resaved.size());
    while(firstDiff < std::size_t(n) && original[firstDiff] == resaved[firstDiff])
      ++firstDiff;
    INFO(
        "original " << original.size() << " bytes, resaved " << resaved.size()
                    << " bytes, first difference at offset " << firstDiff);
    // Compared as a bool: dumping megabytes of binary through Catch2's
    // stringifier trips an assertion in catch_textflow.cpp.
    const bool fixedPoint = (resaved == original);
    CHECK(fixedPoint);
  });
}

// IdContainer<..., Ordered=true>::insert pushes to the front, so a reader that
// adds in stream order reverses the array on every load. The DataStream reader
// compensates by adding in reverse; the JSON reader
// (IntervalModelSerialization.cpp JSONWriter::write) adds in stream order, so
// .score files -- which are JSON -- flip their process order on every open and
// saveAsJson is not a fixed point.
TEST_CASE(
    "interval process order survives a JSON round-trip",
    "[integration][serialization]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval = base_interval(*doc);
    // Three, not two: two processes swap under a reversal AND under any other
    // permutation, so three is the smallest count that names the defect.
    add_processes(*doc, interval, 3);
    const auto before = process_ids(interval);
    REQUIRE(before.size() >= 4u); // the base Scenario process plus the three

    score::Document* reloaded = score::test::reload_via_json(ctx, *doc);
    REQUIRE(reloaded != nullptr);

    CHECK(process_ids(base_interval(*reloaded)) == before);
  });
}

// The byte form of the case above, on the format the corpus is actually stored
// in. DocumentRoundtripTest proves a *fresh* document is a JSON fixed point --
// it holds one process, and a one-element array is its own reverse -- so the
// added processes are what this asserts.
//
// Still red for one reason, and it is view geometry rather than process order.
// The divergence sits at byte ~6684:
//
//   pass1: ..."Zoom":7475113.772947206,"Center":2336705202,...
//   pass2: ..."Zoom":7386348.887164459,"Center":2330696192,...
//
// IntervalModel::m_zoom / m_center are written back from the minimap by
// ScenarioDocumentPresenter::on_minimapChanged, which derives them from the
// live viewport width. The reloaded document lays out in a viewport of a
// different width, so it stores a different -- equally correct -- zoom. Making
// this green means keeping a viewport-driven recomputation from overwriting
// loaded view state, which is a view-behavior change, not a serialization one.
TEST_CASE(
    "a scenario with added processes is a JSON byte fixed point",
    "[integration][serialization][!shouldfail]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    add_processes(*doc, base_interval(*doc), 3);

    const QByteArray pass1 = score::test::save_as_json(*doc);
    score::Document* reloaded = score::test::reload_via_json(ctx, *doc);
    REQUIRE(reloaded != nullptr);
    const QByteArray pass2 = score::test::save_as_json(*reloaded);

    std::size_t firstDiff = 0;
    const auto n = std::min(pass1.size(), pass2.size());
    while(firstDiff < std::size_t(n) && pass1[firstDiff] == pass2[firstDiff])
      ++firstDiff;
    // A byte offset alone does not name the field; the surrounding text does.
    const auto around = [firstDiff](const QByteArray& b) {
      const auto from = firstDiff > 60 ? firstDiff - 60 : 0;
      return b.mid(int(from), 140).toStdString();
    };
    INFO(
        "pass1 " << pass1.size() << " bytes, pass2 " << pass2.size()
                 << " bytes, first difference at offset " << firstDiff
                 << "\npass1: ..." << around(pass1) << "...\npass2: ..."
                 << around(pass2) << "...");
    // Compared as a bool: see the note in the case above.
    const bool fixedPoint = (pass1 == pass2);
    CHECK(fixedPoint);
  });
}
