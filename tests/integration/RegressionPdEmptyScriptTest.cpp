// Regression test for df59191ae — "pd: don't open a patch when the script
// path is empty".
//
// Pd::ProcessModel's constructor calls setScript() with its construction data.
// With no script set (empty path), setScript still called libpd_openfile with
// an empty filename, which makes libpd read out of bounds (caught as a
// heap-buffer-overflow under ASAN). The fix skips the open and the dependent
// processing, leaving the instance patchless.
//
// This test creates a PureData process with empty construction data: without
// the fix it aborts inside libpd_openfile; with it, the process is created
// patchless and the document keeps working.

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

TEST_CASE(
    "Creating a PureData process with an empty script does not crash",
    "[integration][regression][pd][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();

    // The PureData process factory (dynamically loaded score_plugin_pd).
    const auto pd_key = UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("7b3b18ea-311b-40f9-b04e-60ec1fe05786"));
    auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
    auto* factory = factories.get(pd_key);
    REQUIRE(factory != nullptr);

    // Empty construction data => ProcessModel ctor calls setScript(""):
    // the exact code path that read out of bounds in libpd before the fix.
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
        interval, factory->concreteKey(), QString{}, QPointF{});

    Process::ProcessModel* pd = nullptr;
    for(auto& p : interval.processes)
      if(p.concreteKey() == pd_key)
        pd = &p;
    REQUIRE(pd != nullptr);

    // The patchless instance is still a valid process model.
    CHECK(pd->prettyName().size() > 0);
  });
}
