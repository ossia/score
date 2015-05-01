#pragma once
#include <QDockWidget>

namespace iscore
{
    class PanelPresenterInterface;
    class PanelViewInterface : public QObject
    {
        public:
            // NOTE : the objectName here is used for display.
            // TODO make a specific method for this
            PanelViewInterface(QObject* parent):
                    QObject{parent}
            {

            }

            virtual ~PanelViewInterface() = default;
            virtual QWidget* getWidget() = 0;

            virtual Qt::DockWidgetArea defaultDock() const = 0;
            virtual int priority() const = 0; // Higher priority will come up first.
    };
}
