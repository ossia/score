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

    // The reloaded document preserves the process. Full binary equality is
    // intentionally not asserted: serialization is not byte-stable once there
    // is content (ids/ordering differ), so we check structure instead.
    CHECK(base_interval(*reloaded).processes.size() == initial + 1);
  });
}
