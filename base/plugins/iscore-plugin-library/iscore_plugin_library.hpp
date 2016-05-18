#pragma once
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>

class iscore_plugin_library :
    public QObject,
    public iscore::Plugin_QtInterface,
    public iscore::FactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::Plugin_QtInterface
                iscore::FactoryInterface_QtInterface)

    public:
        iscore_plugin_library();
        virtual ~iscore_plugin_library();

    private:
        std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> factories(
                const iscore::ApplicationContext&,
                const iscore::AbstractFactoryKey& factoryName) const override;

        iscore::Version version() const override;
        UuidKey<iscore::Plugin> key() const override;
};
