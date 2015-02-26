#pragma once
#include <QDockWidget>

namespace iscore
{
    class PanelPresenterInterface;
    class PanelViewInterface : public QObject
    {
            Q_OBJECT
        public:
            using QObject::QObject;
            virtual ~PanelViewInterface() = default;
            virtual QWidget* getWidget() = 0;

            virtual Qt::DockWidgetArea defaultDock() const = 0;
    };
}
