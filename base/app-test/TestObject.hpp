#pragma once
#include <core/presenter/DocumentManager.hpp>
#include <core/document/Document.hpp>
#include <core/command/CommandStack.hpp>

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTimer>

class TestObject : public QObject
{
        const iscore::ApplicationContext& m_context;
        Q_OBJECT
    public:
        TestObject(const iscore::ApplicationContext& ctx):m_context{ctx}
        {
            QTimer::singleShot(1000, this, SIGNAL(appStarting()));
            connect(this, SIGNAL(appStarting()),
                    this, SLOT(appStarted()));
        }

    signals:
        void appStarting();

    public slots:
        void appStarted()
        {
            QDirIterator it("testdata/stacks/");
            while (it.hasNext()) {
                qDebug() << it.next();

                auto doc = m_context.documents.loadStack(m_context, it.filePath());
                QApplication::processEvents();
                if(!doc)
                    continue;

                iscore::CommandStack& stack = doc->commandStack();
                while(stack.canUndo())
                {
                    stack.undo();
                    QApplication::processEvents();

                    // TODO select / deslect all events in all processes recursively.
                }
                while(stack.canRedo())
                {
                    stack.redo();
                    QApplication::processEvents();
                }

                auto byte_arr = doc->saveAsByteArray();
                QApplication::processEvents();
                auto json_arr = doc->saveAsJson();
                QApplication::processEvents();

                auto doctype = *m_context.components.availableDocuments().begin();
                auto ba_doc = m_context.documents.loadDocument(m_context, byte_arr, doctype);
                QApplication::processEvents();
                auto json_doc = m_context.documents.loadDocument(m_context, json_arr, doctype);
                QApplication::processEvents();

                m_context.documents.forceCloseDocument(m_context, *doc);
                QApplication::processEvents();
                m_context.documents.forceCloseDocument(m_context, *ba_doc);
                QApplication::processEvents();
                m_context.documents.forceCloseDocument(m_context, *json_doc);
                QApplication::processEvents();
            }

            qApp->exit(0);
        }

};
