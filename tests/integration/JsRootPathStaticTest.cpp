// JS::ProcessModel::rootPath() must not cache the Library settings model in a
// function-local static: the static would bind to the first application of the
// process, and a Javascript process created in a second application (which
// MinimalApplication supports) would read its freed settings. Two test cases,
// each creating a Javascript process in its own application; only reliable
// under ASan, where the use-after-free aborts instead of reading garbage.
//
// In its own executable because an ASan abort would take the rest of a binary
// with it.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <JS/JSProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QPointF>

#include <catch2/catch_test_macros.hpp>

namespace
{
void makeJsProcess(const score::GUIApplicationContext& ctx)
{
  score::Document* doc = score::test::new_document(ctx);
  REQUIRE(doc != nullptr);

  auto& interval
      = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
            .baseInterval();

  auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
  const auto js_key = UuidKey<Process::ProcessModel>::fromString(
      QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0"));
  auto* factory = factories.get(js_key);
  REQUIRE(factory != nullptr);

  CommandDispatcher<> disp{doc->context().commandStack};
  disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
      interval, factory->concreteKey(), factory->customConstructionData(), QPointF{});

  JS::ProcessModel* js = nullptr;
  for(auto& p : interval.processes)
    if(auto* j = qobject_cast<JS::ProcessModel*>(&p))
      js = j;
  REQUIRE(js != nullptr);
  CHECK_FALSE(js->rootPath().isEmpty());
}
}

TEST_CASE("A Javascript process in the first application of a process",
          "[integration][js][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    makeJsProcess(ctx);
  });
}

TEST_CASE("A Javascript process in the second application of a process",
          "[integration][js][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    makeJsProcess(ctx);
  });
}
