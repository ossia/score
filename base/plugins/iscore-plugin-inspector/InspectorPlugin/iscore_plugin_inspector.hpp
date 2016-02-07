#pragma once
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <QObject>
#include <vector>

namespace iscore {
class FactoryListInterface;
class PanelFactory;
}  // namespace iscore

// RENAMEME
class iscore_plugin_inspector :
    public QObject,
        public iscore::Plugin_QtInterface,
    public iscore::PanelFactory_QtInterface,
    public iscore::FactoryList_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PanelFactory_QtInterface_iid)
        Q_INTERFACES(
                iscore::Plugin_QtInterface
                     iscore::PanelFactory_QtInterface
                     iscore::FactoryList_QtInterface)

    public:
        iscore_plugin_inspector();

        // Panel interface
        std::vector<iscore::PanelFactory*> panels() override;

        // Factory for inspector widgets
        std::vector<std::unique_ptr<iscore::FactoryListInterface>> factoryFamilies() override;

        iscore::Version version() const override;
        UuidKey<iscore::Plugin> key() const override;
};
