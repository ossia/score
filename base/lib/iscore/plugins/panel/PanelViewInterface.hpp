#pragma once
#include <QDockWidget>

namespace iscore
{
    class PanelPresenterInterface;
    class PanelViewInterface : public QObject
    {
        public:
            using QObject::QObject;

            virtual ~PanelViewInterface() = default;
            virtual QWidget* getWidget() = 0;

            virtual Qt::DockWidgetArea defaultDock() const = 0;
            virtual int priority() const = 0; // Higher priority will come up first.
            virtual QString prettyName() const = 0;
    };
}
