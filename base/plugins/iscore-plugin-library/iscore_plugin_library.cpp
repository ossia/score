#include "iscore_plugin_library.hpp"

#include <Library/Panel/LibraryPanelFactory.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>

iscore_plugin_library::iscore_plugin_library()
{
}

iscore_plugin_library::~iscore_plugin_library()
{

}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_library::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
            TL<
              FW<iscore::PanelDelegateFactory,
                  Library::PanelDelegateFactory>
            >
    >(ctx, key);
}


iscore::Version iscore_plugin_library::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_library::key() const
{
    return "f019a413-0ffd-417f-966a-a824548aca79";
}
