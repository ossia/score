// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score_integration.hpp>

#include <Scenario/Application/ScenarioActions.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <Process/ProcessList.hpp>
#include <QDirIterator>
#include <QLocale>
#include <clocale>
#include <thread>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>

static void run_test()
{
  const auto& ctx = score::GUIAppContext();

  // Create an empty document
  {
    auto& docs = ctx.interfaces<score::DocumentDelegateList>();
    SCORE_ASSERT(!docs.empty());
    auto doc = ctx.docManager.newDocument(ctx, Id<score::DocumentModel>{}, *docs.begin());
    QApplication::processEvents();
    QApplication::processEvents();

    SCORE_ASSERT(doc);
    SCORE_ASSERT(&doc->model().modelDelegate());
    auto& doc_pm = doc->model().modelDelegate();
    auto& scenario_dm = safe_cast<Scenario::ScenarioDocumentModel&>(doc_pm);

    score::CommandStack& stack = doc->commandStack();
    for(auto& process: ctx.interfaces<Process::ProcessFactoryList>())
    {
      CommandDispatcher<> disp{doc->context().commandStack};
      disp.submit<Scenario::Command::AddProcessToInterval>(scenario_dm.baseInterval(), process.concreteKey(), process.customConstructionData());
    }

    for (int i = 0; i < 10; i++)
    {
      QApplication::processEvents();
    }

    ctx.actions.action<Actions::Play>().action()->trigger();

    QTimer* t = new QTimer;
    t->setInterval(5000);
    t->setSingleShot(true);

    t->start();
    QObject::connect(t, &QTimer::timeout, [=, &ctx] {
      ctx.actions.action<Actions::Stop>().action()->trigger();
      for (int i = 0; i < 30; i++)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        QApplication::processEvents();
      }

      auto doc = ctx.docManager.currentDocument();
      ctx.docManager.forceCloseDocument(ctx, *doc);
      for (int i = 0; i < 30; i++)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        QApplication::processEvents();
      }

      t->deleteLater();
      qApp->exit(0);
    });
  }
}

int main(int argc, char** argv)
{
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  score::MinimalGUIApplication app(argc, argv);

  QMetaObject::invokeMethod(&app, run_test, Qt::QueuedConnection);

  return app.exec();
}
