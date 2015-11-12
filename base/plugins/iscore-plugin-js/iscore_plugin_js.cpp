#include "iscore_plugin_js.hpp"

#include <JS/JSProcessModel.hpp>
#include <JS/JSProcessFactory.hpp>

#include <JS/Inspector/JSInspectorFactory.hpp>
#include <JS/Commands/EditScript.hpp>

iscore_plugin_js::iscore_plugin_js() :
    QObject {}
{
}

std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_js::factories(const std::string& factoryName) const
{
    if(factoryName == ProcessFactory::staticFactoryName())
    {
        return {new JSProcessFactory};
    }

    if(factoryName == InspectorWidgetFactory::staticFactoryName())
    {
        return {new JSInspectorFactory};
    }

    return {};
}

std::pair<const std::string, CommandGeneratorMap> iscore_plugin_js::make_commands()
{
    std::pair<const std::string, CommandGeneratorMap> cmds{JSCommandFactoryName(), CommandGeneratorMap{}};
    boost::mpl::for_each<
            boost::mpl::list<
            EditScript
            >,
            boost::type<boost::mpl::_>
            >(CommandGeneratorMapInserter{cmds.second});

    return cmds;
}
