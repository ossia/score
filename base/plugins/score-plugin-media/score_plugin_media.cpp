#include "score_plugin_media.hpp"
#include <Media/Sound/SoundFactory.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <QAction>
#include <Media/Inspector/Factory.hpp>

#include <Media/Input/InputFactory.hpp>
#include <Media/Sound/SoundFactory.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>
#include <Media/Inspector/Factory.hpp>
#include <Media/ApplicationPlugin.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/FactorySetup.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <score_plugin_media_commands_files.hpp>

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_media::make_commands()
{
    using namespace Media::Commands;
    std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{Media::CommandFactoryName(), CommandGeneratorMap{}};

    using Types = TypeList<
#include <score_plugin_media_commands.hpp>
      >;
    for_each_type<Types>(score::commands::FactoryInserter{cmds.second});

    return cmds;
}

score::ApplicationPlugin*score_plugin_media::make_applicationPlugin(const score::ApplicationContext& app)
{
  return new Media::ApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_media::factories(
        const score::ApplicationContext& ctx,
        const score::InterfaceKey& key) const
{
    return instantiate_factories<
            score::ApplicationContext,
        FW<Process::ProcessModelFactory
            , Media::Sound::ProcessFactory
            , Media::Input::ProcessFactory
            >,
        FW<Inspector::InspectorWidgetFactory
            , Media::Sound::InspectorFactory
            , Media::Input::InspectorFactory
            >,
        FW<Process::LayerFactory
          , Media::Sound::LayerFactory
          , Media::Input::LayerFactory
            >,
        FW<Scenario::DropHandler,
            Media::Sound::DropHandler>,
        FW<Scenario::IntervalDropHandler,
            Media::Sound::IntervalDropHandler>
    >(ctx, key);
}

score_plugin_media::score_plugin_media()
{

}

score_plugin_media::~score_plugin_media()
{

}
