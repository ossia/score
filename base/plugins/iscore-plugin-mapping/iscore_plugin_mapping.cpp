#include <Mapping/MappingColors.hpp>
#include <Mapping/MappingLayerModel.hpp>
#include <Mapping/MappingModel.hpp>
#include <Mapping/MappingPresenter.hpp>
#include <Mapping/MappingView.hpp>

#include <unordered_map>

#include "Inspector/InspectorWidgetFactoryInterface.hpp"
#include <Mapping/Commands/MappingCommandFactory.hpp>
#include <Mapping/MappingProcessMetadata.hpp>
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "iscore_plugin_mapping.hpp"

#include <Curve/Process/CurveProcessFactory.hpp>
DEFINE_CURVE_PROCESS_FACTORY(
        MappingFactory,
        MappingProcessMetadata,
        MappingModel,
        MappingLayerModel,
        MappingPresenter,
        MappingView,
        MappingColors)


#if defined(ISCORE_LIB_INSPECTOR)
#include <Mapping/Inspector/MappingInspectorFactory.hpp>
#endif

#include <iscore_plugin_mapping_commands_files.hpp>

iscore_plugin_mapping::iscore_plugin_mapping() :
    QObject {}
{
}

std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_mapping::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& factoryName) const
{
    if(factoryName == ProcessFactory::staticFactoryKey())
    {
        return {new MappingFactory};
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

    using Types = iscore::commands::TypeList<
#include <iscore_plugin_mapping_commands.hpp>
      >;
    iscore::commands::ForEach<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}
