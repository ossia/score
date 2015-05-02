#pragma once
#include <QObject>
#include <QStringList>

namespace iscore
{
    class PanelFactoryInterface;
    class PanelFactoryInterface_QtInterface
    {
        public:
            virtual ~PanelFactoryInterface_QtInterface() = default;

            virtual QList<PanelFactoryInterface*> panels() = 0;
    };
}

#define PanelFactoryInterface_QtInterface_iid "org.ossia.i-score.plugins.PanelFactoryInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::PanelFactoryInterface_QtInterface, PanelFactoryInterface_QtInterface_iid)
