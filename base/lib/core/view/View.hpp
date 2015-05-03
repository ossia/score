#pragma once
#include <QMainWindow>
#include <QDebug>
#include <QMenuBar>
#include <set>

#include <core/presenter/Action.hpp>
namespace iscore
{
    class PanelView;
    class Document;
    class DocumentView;
    /**
     * @brief The View class
     *
     * The main display of the application.
     */
    class View : public QMainWindow
    {
            Q_OBJECT
        public:
            View(QObject* parent);

            void addDocumentView(iscore::DocumentView*);
            void setupPanelView(PanelView* v);

            void closeDocument(iscore::DocumentView* doc);

        signals:
            /**
             * @brief insertActionIntoMenubar
             *
             * A quick signal to add an action.
             * Especially considering that we already know the presenter.
             */
            void insertActionIntoMenubar(PositionedMenuAction);

            void activeDocumentChanged(Document*);
            void closeRequested(Document*);


        private:
            QList<QPair<PanelView*, QDockWidget*>> m_leftWidgets;
            QList<QPair<PanelView*, QDockWidget*>> m_rightWidgets;

            QTabWidget* m_tabWidget{};
    };
}
