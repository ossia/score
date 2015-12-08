#include "CSPPlugin.hpp"
#include "CSPApplicationPlugin.hpp"
#include "MoveEventCSPFactory.hpp"
#include "MoveEventCSPFlexFactory.hpp"

iscore_plugin_csp::iscore_plugin_csp() :
    QObject {}
{
}

iscore::GUIApplicationContextPlugin* iscore_plugin_csp::make_applicationPlugin(
        const iscore::ApplicationContext& app)
{
    return new CSPApplicationPlugin{app};
}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>>
iscore_plugin_csp::factories(
        const iscore::ApplicationContext&,
        const iscore::FactoryBaseKey& factoryName) const
{
    if(factoryName == MoveEventFactoryInterface::staticFactoryKey())
    {
        return make_ptr_vector<iscore::FactoryInterfaceBase,
                MoveEventCSPFactory,
                MoveEventCSPFlexFactory>();
    }

    return {};
}
