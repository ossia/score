// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score_integration.hpp>

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QLocale>
#include <clocale>

static void run_test()
{
  const auto& ctx = score::GUIAppContext();
  auto doc = ctx.docManager.loadFile(
      ctx, "testdata/execution.scorejson");
  qApp->processEvents();
  auto& doc_pm = doc->model().modelDelegate();
  auto& scenario_dm = static_cast<Scenario::ScenarioDocumentModel&>(doc_pm);

  auto& scenar = static_cast<Scenario::ProcessModel&>(
      *scenario_dm.baseInterval().processes.begin());
  for (auto& elt : scenar.intervals)
  {
    doc->context().selectionStack.pushNewSelection({&elt});
    qApp->processEvents();
  }
  for (auto& elt : scenar.events)
  {
    doc->context().selectionStack.pushNewSelection({&elt});
    qApp->processEvents();
  }
  for (auto& elt : scenar.states)
  {
    doc->context().selectionStack.pushNewSelection({&elt});
    qApp->processEvents();
  }
  for (auto& elt : scenar.timeSyncs)
  {
    doc->context().selectionStack.pushNewSelection({&elt});
    qApp->processEvents();
  }

  ctx.docManager.forceCloseDocument(ctx, *doc);
  QApplication::processEvents();
}

SCORE_INTEGRATION_TEST(run_test)
