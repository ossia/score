// JS::ProcessModel::rootPath() (JSProcessModel.cpp:170) caches the Library
// settings model in a function-local static reference:
//
//   static const auto& lib = score::AppContext().settings<Library::Settings::Model>();
//
// The static binds to whichever application first reached that line and is
// never rebound. Build a second score application in the same process -- which
// MinimalApplication supports and which test_regression_minimal_app_twice
// exists to guarantee -- and creating a Javascript process reads the first
// application's freed settings:
//
//   ERROR: AddressSanitizer: heap-use-after-free
//     #6 Library::Settings::Model::getDefaultLibraryPath() LibrarySettings.cpp:156
//     #7 JS::ProcessModel::rootPath()                      JSProcessModel.cpp:172
//     #8 JS::ComponentCache::getExecution()                JSProcessModel.cpp:726
//     ...
//     #11 JS::ProcessModel::ProcessModel(...)              JSProcessModel.cpp:77
//   freed by:
//     #1 Library::Settings::Model::~Model()                LibrarySettings.hpp:45
//     #2 score::Settings::teardownModels()                 Settings.cpp:31
//     #3 score::MinimalGUIApplication::~MinimalGUIApplication()
//
// Two test cases, each creating a Javascript process in its own application:
// the first passes, the second reads freed memory. That is also why
// JsProcessUiTest is a single TEST_CASE.
//
// Its own executable and WILL_FAIL because ASan aborts the process, so nothing
// after it in a binary is reported. When rootPath() stops caching, this passes
// and the ctest entry goes red -- the signal to drop WILL_FAIL.

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
