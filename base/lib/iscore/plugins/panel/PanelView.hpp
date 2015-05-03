#pragma once
#include <QDockWidget>

namespace iscore
{
    class PanelPresenter;
    class PanelView : public QObject
    {
        public:
            using QObject::QObject;

            virtual QWidget* getWidget() = 0;

            virtual Qt::DockWidgetArea defaultDock() const = 0;
            virtual int priority() const = 0; // Higher priority will come up first.
            virtual QString prettyName() const = 0;
    };
}
