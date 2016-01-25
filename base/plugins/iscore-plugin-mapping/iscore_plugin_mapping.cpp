#include <Mapping/MappingColors.hpp>
#include <Mapping/MappingLayerModel.hpp>
#include <Mapping/MappingModel.hpp>
#include <Mapping/MappingPresenter.hpp>
#include <Mapping/MappingView.hpp>

#include <unordered_map>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Mapping/Commands/MappingCommandFactory.hpp>
#include <Mapping/MappingProcessMetadata.hpp>
#include <Process/ProcessFactory.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include "iscore_plugin_mapping.hpp"

#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <Curve/Process/CurveProcessFactory.hpp>

namespace Mapping
{
DEFINE_CURVE_PROCESS_FACTORY(
        MappingFactory,
        Mapping::MappingProcessMetadata,
        Mapping::MappingModel,
        Mapping::MappingLayerModel,
        Mapping::MappingPresenter,
        Mapping::MappingView,
        Mapping::MappingColors)
}


#if defined(ISCORE_LIB_INSPECTOR)
#include <Mapping/Inspector/MappingInspectorFactory.hpp>
#endif

#include <iscore_plugin_mapping_commands_files.hpp>

iscore_plugin_mapping::iscore_plugin_mapping() :
    QObject {}
{
}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_mapping::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::AbstractFactoryKey& key) const
{
    using namespace Mapping;
    return instantiate_factories<
            iscore::ApplicationContext,
            TL<
            FW<Process::ProcessFactory,
                Mapping::MappingFactory>,
            FW<ProcessInspectorWidgetDelegateFactory,
                MappingInspectorFactory>
            >>(ctx, key);
}

std::pair<const CommandParentFactoryKey, CommandGeneratorMap> iscore_plugin_mapping::make_commands()
{
    using namespace Mapping;
    std::pair<const CommandParentFactoryKey, CommandGeneratorMap> cmds{MappingCommandFactoryName(), CommandGeneratorMap{}};

    using Types = TypeList<
#include <iscore_plugin_mapping_commands.hpp>
      >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}
