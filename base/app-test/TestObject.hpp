#pragma once
#include <core/presenter/DocumentManager.hpp>
#include <core/document/Document.hpp>
#include <core/command/CommandStack.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/actions/ActionManager.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <core/document/DocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTimer>
#include <thread>

class TestObject : public QObject
{
        const score::GUIApplicationContext& m_context;
        Q_OBJECT
    public:
        TestObject(const score::GUIApplicationContext& ctx):
            m_context{ctx}
        {
            QTimer::singleShot(1000, this, SIGNAL(appStarting()));
            connect(this, SIGNAL(appStarting()),
                    this, SLOT(appStarted()));
        }

    Q_SIGNALS:
        void appStarting();

    public Q_SLOTS:
        void appStarted()
        {
            QDirIterator it("testdata/stacks/");

            while (it.hasNext())
            {
                it.next();
                if(!it.filePath().contains(".stack"))
                {
                  continue;
                }

                qDebug() << "Loading stack" << it.fileName();
                auto doc = m_context.docManager.loadStack(m_context, it.filePath());
                QApplication::processEvents();
                if(!doc)
                    continue;

                score::CommandStack& stack = doc->commandStack();
                while(stack.canUndo())
                {
                    stack.undo(ctx);
                    QApplication::processEvents();

                    // TODO select / deslect all events in all processes recursively.
                }
                while(stack.canRedo())
                {
                    stack.redo(ctx);
                    QApplication::processEvents();
                }

                auto byte_arr = doc->saveAsByteArray();
                QApplication::processEvents();
                auto json_arr = doc->saveAsJson();
                QApplication::processEvents();

                m_context.docManager.forceCloseDocument(m_context, *doc);
                QApplication::processEvents();

                auto& doctype = *m_context.interfaces<score::DocumentDelegateList>().begin();

                auto ba_doc = m_context.docManager.loadDocument(m_context, byte_arr, doctype);
                QApplication::processEvents();
                m_context.docManager.forceCloseDocument(m_context, *ba_doc);
                QApplication::processEvents();

                auto json_doc = m_context.docManager.loadDocument(m_context, json_arr, doctype);
                QApplication::processEvents();
                m_context.docManager.forceCloseDocument(m_context, *json_doc);
                QApplication::processEvents();
            }

            {
                auto doc = m_context.docManager.loadFile(m_context, "testdata/execution.scorejson");
                if(!doc)
                  qApp->exit(1);
                for(int i = 0; i < 10; i++)
                {
                    QApplication::processEvents();
                }

                m_context.actions.action<Actions::Play>().action()->trigger();

                QTimer* t = new QTimer;
                t->setInterval(5000);
                t->setSingleShot(true);

                t->start();
                QObject::connect(t, &QTimer::timeout, this, [=] () {
                    m_context.actions.action<Actions::Stop>().action()->trigger();
                    for(int i = 0; i < 30; i++)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        QApplication::processEvents();
                    }

                    m_context.docManager.forceCloseDocument(m_context, *doc);
                    for(int i = 0; i < 30; i++)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        QApplication::processEvents();
                    }

                    selectionTest();
                });
            }
        }


        void selectionTest()
        {
            auto doc = m_context.docManager.loadFile(m_context, "testdata/execution.scorejson");
            qApp->processEvents();
            auto& doc_pm = doc->model().modelDelegate();
            auto& scenario_dm = static_cast<Scenario::ScenarioDocumentModel&>(doc_pm);

            auto& scenar = static_cast<Scenario::ProcessModel&>(*scenario_dm.baseInterval().processes.begin());
            for(auto& elt : scenar.intervals)
            {
                doc->context().selectionStack.pushNewSelection({&elt});
                qApp->processEvents();
            }
            for(auto& elt : scenar.events)
            {
                doc->context().selectionStack.pushNewSelection({&elt});
                qApp->processEvents();
            }
            for(auto& elt : scenar.states)
            {
                doc->context().selectionStack.pushNewSelection({&elt});
                qApp->processEvents();
            }
            for(auto& elt : scenar.timeSyncs)
            {
                doc->context().selectionStack.pushNewSelection({&elt});
                qApp->processEvents();
            }


            m_context.docManager.forceCloseDocument(m_context, *doc);
            QApplication::processEvents();

            qApp->exit(0);
        }

};
