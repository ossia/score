#pragma once
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
class iscore_plugin_library :
    public QObject,
        public iscore::Plugin_QtInterface,
    public iscore::PanelFactory_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PanelFactory_QtInterface_iid)
        Q_INTERFACES(
                iscore::Plugin_QtInterface
                     iscore::PanelFactory_QtInterface)

    public:
        iscore_plugin_library();
        virtual ~iscore_plugin_library();

    private:
        std::vector<iscore::PanelFactory*> panels() override;

        int32_t version() const override;
        UuidKey<iscore::Plugin> key() const override;
};
