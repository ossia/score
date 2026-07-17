// Regression test for ca4b2b6d3 — "threedim: keep Splat process ports stable
// across reload".
//
// Splat::Model::init() appended the "Camera" projection combo-box inlet
// OUTSIDE the "ports are empty" guard, so every deserialization re-added it:
// each save/load cycle grew the inlet count by one. The fix moves it inside
// the guard so the ports are created exactly once.
//
// Without the fix, the first reload yields N+1 inlets and the second N+2;
// with it, the count is stable.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Process.hpp>
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
static const auto splat_key = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("cdc15a16-e856-4e02-9339-7d9e48da10ce"));

Scenario::IntervalModel& base_interval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}

Process::ProcessModel* find_splat(score::Document& doc)
{
  for(auto& p : base_interval(doc).processes)
    if(p.concreteKey() == splat_key)
      return &p;
  return nullptr;
}
}

TEST_CASE(
    "Splat process ports stay stable across save/reload cycles",
    "[integration][regression][threedim][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    // The Splat process factory (dynamically loaded score_plugin_threedim).
    auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
    auto* factory = factories.get(splat_key);
    REQUIRE(factory != nullptr);

    // Create a Splat process with empty construction data.
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
        base_interval(*doc), factory->concreteKey(), QString{}, QPointF{});

    Process::ProcessModel* splat = find_splat(*doc);
    REQUIRE(splat != nullptr);

    const auto inlets = splat->inlets().size();
    const auto outlets = splat->outlets().size();
    REQUIRE(inlets > 0);

    // First reload: without the fix the "Camera" combo box is re-appended and
    // the inlet count grows by one.
    score::Document* reloaded = score::test::reload_via_bytes(ctx, *doc, "splat-1");
    REQUIRE(reloaded != nullptr);
    Process::ProcessModel* splat1 = find_splat(*reloaded);
    REQUIRE(splat1 != nullptr);
    CHECK(splat1->inlets().size() == inlets);
    CHECK(splat1->outlets().size() == outlets);

    // Second reload: without the fix the count keeps growing (N+2).
    score::Document* reloaded2
        = score::test::reload_via_bytes(ctx, *reloaded, "splat-2");
    REQUIRE(reloaded2 != nullptr);
    Process::ProcessModel* splat2 = find_splat(*reloaded2);
    REQUIRE(splat2 != nullptr);
    CHECK(splat2->inlets().size() == inlets);
    CHECK(splat2->outlets().size() == outlets);
  });
}
