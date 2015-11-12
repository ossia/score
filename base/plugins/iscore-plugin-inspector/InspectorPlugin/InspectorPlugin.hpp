#pragma once
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>

class InspectorControl;
class iscore_plugin_inspector :
    public QObject,
    public iscore::PanelFactory_QtInterface,
    public iscore::PluginControlInterface_QtInterface,
    public iscore::FactoryFamily_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PanelFactory_QtInterface_iid)
        Q_INTERFACES(
                     iscore::PanelFactory_QtInterface
                     iscore::FactoryFamily_QtInterface
                     iscore::PluginControlInterface_QtInterface)

    public:
        iscore_plugin_inspector();

        // Panel interface
        QList<iscore::PanelFactory*> panels() override;

        // Offre la factory de Inspector
        std::vector<iscore::FactoryFamily> factoryFamilies() override;

        iscore::PluginControlInterface* make_control(iscore::Presenter*) override;

    private:
        InspectorControl* m_inspectorControl{};
};
