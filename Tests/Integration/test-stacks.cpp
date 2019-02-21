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

#include <QDirIterator>
#include <QLocale>
#include <clocale>
#include <thread>

static void run_test()
{
  const auto& ctx = score::GUIAppContext();
  QDirIterator it("testdata/stacks/");

  while (it.hasNext())
  {
    it.next();
    if (!it.filePath().contains(".stack"))
    {
      continue;
    }

    qDebug() << "Loading stack" << it.fileName();
    auto doc = ctx.docManager.loadStack(ctx, it.filePath());
    QApplication::processEvents();
    if (!doc)
      continue;

    score::CommandStack& stack = doc->commandStack();
    while (stack.canUndo())
    {
      stack.undo();
      QApplication::processEvents();

      // TODO select / deslect all events in all processes recursively.
    }
    while (stack.canRedo())
    {
      stack.redo();
      QApplication::processEvents();
    }

    auto byte_arr = doc->saveAsByteArray();
    QApplication::processEvents();
    auto json_arr = doc->saveAsJson();
    QApplication::processEvents();

    ctx.docManager.forceCloseDocument(ctx, *doc);
    QApplication::processEvents();

    auto& doctype
        = *ctx.interfaces<score::DocumentDelegateList>().begin();

    auto ba_doc
        = ctx.docManager.loadDocument(ctx, it.fileName(), byte_arr, doctype);
    QApplication::processEvents();
    ctx.docManager.forceCloseDocument(ctx, *ba_doc);
    QApplication::processEvents();

    auto json_doc
        = ctx.docManager.loadDocument(ctx, it.fileName(), json_arr, doctype);
    QApplication::processEvents();
    ctx.docManager.forceCloseDocument(ctx, *json_doc);
    QApplication::processEvents();
  }

  {
    auto doc = ctx.docManager.loadFile(
        ctx, "testdata/execution.scorejson");
    if (!doc)
      qApp->exit(1);
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

      ctx.docManager.forceCloseDocument(ctx, *doc);
      for (int i = 0; i < 30; i++)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        QApplication::processEvents();
      }
    });
  }

  qApp->exit(0);
}

int main(int argc, char** argv)
{
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  score::MinimalGUIApplication app(argc, argv);

  QMetaObject::invokeMethod(&app, run_test, Qt::QueuedConnection);

  return app.exec();
}
