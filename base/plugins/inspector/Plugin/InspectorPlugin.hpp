#pragma once
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>

class InspectorControl;
class InspectorPlugin :
    public QObject,
    public iscore::PanelFactoryInterface_QtInterface,
    public iscore::PluginControlInterface_QtInterface,
    public iscore::FactoryFamily_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PanelFactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                     iscore::PanelFactoryInterface_QtInterface
                     iscore::FactoryFamily_QtInterface
                     iscore::PluginControlInterface_QtInterface)

    public:
        InspectorPlugin();

        // Panel interface
        QList<iscore::PanelFactoryInterface*> panels() override;

        // Offre la factory de Inspector
        QVector<iscore::FactoryFamily> factoryFamilies() override;

        iscore::PluginControlInterface* control() override;

    private:
        InspectorControl* m_inspectorControl;
};
