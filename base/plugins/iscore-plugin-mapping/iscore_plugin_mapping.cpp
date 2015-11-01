#include "iscore_plugin_mapping.hpp"

#include <Curve/Process/CurveProcessFactory.hpp>

#include <Mapping/MappingColors.hpp>
#include <Mapping/MappingModel.hpp>
#include <Mapping/MappingView.hpp>
#include <Mapping/MappingLayerModel.hpp>
#include <Mapping/MappingPresenter.hpp>

DEFINE_CURVE_PROCESS_FACTORY(
        MappingFactory,
        "Mapping",
        MappingModel,
        MappingLayerModel,
        MappingPresenter,
        MappingView,
        MappingColors)


#if defined(ISCORE_LIB_INSPECTOR)
#include "Mapping/Inspector/MappingInspectorFactory.hpp"
#endif


iscore_plugin_mapping::iscore_plugin_mapping() :
    QObject {}
{
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
    /*
    std::pair<const std::string, CommandGeneratorMap> cmds{MappingCommandFactoryName(), CommandGeneratorMap{}};
    boost::mpl::for_each<
            boost::mpl::list<
            >,
            boost::type<boost::mpl::_>
            >(CommandGeneratorMapInserter{cmds.second});

    return cmds;
    */
    return {};
}
