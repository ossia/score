#pragma once
#include <QDockWidget>

namespace iscore
{
    class PanelPresenter;
    struct DefaultPanelStatus
    {
            DefaultPanelStatus(bool isShown, Qt::DockWidgetArea d, int prio, QString name):
                shown{isShown},
                dock{d},
                priority{prio},
                prettyName{name}
            {}

          const bool shown;
          const Qt::DockWidgetArea dock;
          const int priority;  // Higher priority will come up first.
          const QString prettyName;
    };

    class PanelView : public QObject
    {
        public:
            using QObject::QObject;

            virtual ~PanelView();

            virtual QWidget* getWidget() = 0;

            virtual const DefaultPanelStatus& defaultPanelStatus() const = 0;

            virtual const QString shortcut() const = 0; // TODO put me in defaultPanelStatus
    };
}
