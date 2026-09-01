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

namespace
{
Scenario::IntervalModel& base_interval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
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
