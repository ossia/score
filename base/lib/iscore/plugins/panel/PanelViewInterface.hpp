#pragma once
#include <QDockWidget>

namespace iscore
{
    class PanelPresenterInterface;
    class PanelViewInterface : public QObject
    {
            Q_OBJECT
        public:
            // NOTE : the objectName here is used for display.
            // TODO make it a namedobject
            using QObject::QObject;
            virtual ~PanelViewInterface() = default;
            virtual QWidget* getWidget() = 0;

            virtual Qt::DockWidgetArea defaultDock() const = 0;
    };
}
