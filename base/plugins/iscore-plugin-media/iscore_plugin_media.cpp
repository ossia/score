#include "iscore_plugin_media.hpp"
#include <Media/Sound/SoundFactory.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <QAction>
#include <Media/Inspector/Factory.hpp>

#include <Media/Sound/SoundFactory.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>
#include <Media/Inspector/Factory.hpp>

#include <iscore/plugins/application/GUIApplicationPlugin.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore_plugin_media_commands_files.hpp>

std::pair<const CommandGroupKey, CommandGeneratorMap> iscore_plugin_media::make_commands()
{
    using namespace Media::Commands;
    std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{Media::CommandFactoryName(), CommandGeneratorMap{}};

    using Types = TypeList<
#include <iscore_plugin_media_commands.hpp>
      >;
    for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

    return cmds;
}

std::vector<std::unique_ptr<iscore::InterfaceBase>> iscore_plugin_media::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::InterfaceKey& key) const
{
    return instantiate_factories<
            iscore::ApplicationContext,
        FW<Process::ProcessModelFactory,
            Media::Sound::ProcessFactory
            >,
        FW<Inspector::InspectorWidgetFactory,
            Media::Sound::InspectorFactory
            >,
        FW<Process::LayerFactory,
            Media::Sound::LayerFactory
            >,
        FW<Scenario::DropHandler,
            Media::Sound::DropHandler>,
        FW<Scenario::ConstraintDropHandler,
            Media::Sound::ConstraintDropHandler>
    >(ctx, key);
}

iscore_plugin_media::iscore_plugin_media()
{

}

iscore_plugin_media::~iscore_plugin_media()
{

}
