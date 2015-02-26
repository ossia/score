#pragma once
#include <QMainWindow>
#include <QDebug>
#include <QMenuBar>
#include <set>

#include <core/presenter/Action.hpp>
namespace iscore
{
    class PanelViewInterface;
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
            View (QObject* parent);

            void setCentralView (iscore::DocumentView*);
            void setupPanelView (PanelViewInterface* v);

            void addSidePanel (QWidget* widg, QString name, Qt::DockWidgetArea);

        signals:
            /**
             * @brief insertActionIntoMenubar
             *
             * A quick signal to add an action.
             * Especially considering that we already know the presenter.
             */
            void insertActionIntoMenubar (PositionedMenuAction);
    };
}
