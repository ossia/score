#pragma once
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>

class InspectorControl;
class iscore_plugin_inspector :
    public QObject,
    public iscore::PanelFactory_QtInterface,
    public iscore::FactoryList_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PanelFactory_QtInterface_iid)
        Q_INTERFACES(
                     iscore::PanelFactory_QtInterface
                     iscore::FactoryList_QtInterface)

    public:
        iscore_plugin_inspector();

        // Panel interface
        std::vector<iscore::PanelFactory*> panels() override;

        // Factory for inspector widgets
        std::vector<iscore::FactoryListInterface*> factoryFamilies() override;
};
