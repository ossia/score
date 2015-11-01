#include "iscore_plugin_mapping.hpp"

#include "Mapping/MappingFactory.hpp"
#include "MappingControl.hpp"

#if defined(ISCORE_LIB_INSPECTOR)
#include "Mapping/Inspector/MappingInspectorFactory.hpp"
#endif


iscore_plugin_mapping::iscore_plugin_mapping() :
    QObject {}
{
}

iscore::PluginControlInterface* iscore_plugin_mapping::make_control(
        iscore::Presenter* pres)
{
    return new MappingControl{pres};
}

QVector<iscore::FactoryInterface*> iscore_plugin_mapping::factories(const QString& factoryName)
{
    if(factoryName == ProcessFactory::factoryName())
    {
        return {new MappingFactory};
    }

#if defined(ISCORE_LIB_INSPECTOR)
    if(factoryName == InspectorWidgetFactory::factoryName())
    {
        return {new MappingInspectorFactory};
    }
#endif
    return {};
}

std::pair<const std::string, CommandGeneratorMap> iscore_plugin_mapping::make_commands()
{
    std::pair<const std::string, CommandGeneratorMap> cmds;
    boost::mpl::for_each<
            boost::mpl::list<
            >,
            boost::type<boost::mpl::_>
            >(CommandGeneratorMapInserter{cmds.second});

    return cmds;
}
