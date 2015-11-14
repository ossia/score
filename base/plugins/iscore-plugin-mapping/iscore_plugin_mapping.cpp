#include "iscore_plugin_mapping.hpp"

#include <Curve/Process/CurveProcessFactory.hpp>

#include <Mapping/MappingColors.hpp>
#include <Mapping/MappingModel.hpp>
#include <Mapping/MappingView.hpp>
#include <Mapping/MappingLayerModel.hpp>
#include <Mapping/MappingPresenter.hpp>

#include <Mapping/Commands/ChangeAddresses.hpp>
#include <Mapping/Commands/MinMaxCommands.hpp>

DEFINE_CURVE_PROCESS_FACTORY(
        MappingFactory,
        MappingProcessMetadata,
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

std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_mapping::factories(const iscore::FactoryBaseKey& factoryName) const
{
    static Curve::EditionSettings set;
    if(factoryName == ProcessFactory::staticFactoryKey())
    {
        return {new MappingFactory{set}};
    }

#if defined(ISCORE_LIB_INSPECTOR)
    if(factoryName == InspectorWidgetFactory::staticFactoryKey())
    {
        return {new MappingInspectorFactory};
    }
#endif
    return {};
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_mapping::make_commands()
{
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{MappingCommandFactoryName(), CommandGeneratorMap{}};
    boost::mpl::for_each<
            boost::mpl::list<
            ChangeSourceAddress,
            ChangeTargetAddress,
            SetMappingSourceMin,
            SetMappingSourceMax,
            SetMappingTargetMin,
            SetMappingTargetMax
            >,
            boost::type<boost::mpl::_>
            >(CommandGeneratorMapInserter{cmds.second});

    return cmds;
}
