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
#include <Media/Effect/EffectProcessFactory.hpp>
#include <Media/Effect/Effect/EffectFactory.hpp>
#include <Media/Effect/LV2/LV2EffectModel.hpp>
#include <Media/Effect/Inspector/EffectInspector.hpp>
#include <Media/Sound/SoundComponent.hpp>


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
            , Media::Effect::ProcessFactory
            >,
        FW<Inspector::InspectorWidgetFactory
            , Media::Sound::InspectorFactory
            , Media::Input::InspectorFactory
            , Media::Effect::InspectorFactory
            >,
        FW<Process::LayerFactory
          , Media::Sound::LayerFactory
          , Media::Input::LayerFactory
          , Media::Effect::LayerFactory
            >,

        FW<Engine::Execution::ProcessComponentFactory
          , Engine::Execution::SoundComponentFactory
          , Engine::Execution::InputComponentFactory
          , Engine::Execution::EffectComponentFactory
        >,
        FW<Media::Effect::EffectFactory
    #if defined(HAS_FAUST)
              , Media::Effect::FaustEffectFactory
    # endif
    #if defined(LILV_SHARED)
                , Media::LV2::LV2EffectFactory
    #endif
                >,
        FW<Scenario::DropHandler,
            Media::Sound::DropHandler>,
        FW<Scenario::IntervalDropHandler,
            Media::Sound::IntervalDropHandler>
    >(ctx, key);
}

std::vector<std::unique_ptr<score::InterfaceListBase> > score_plugin_media::factoryFamilies()
{
    return make_ptr_vector<score::InterfaceListBase,
            Media::Effect::EffectFactoryList>();
}

score_plugin_media::score_plugin_media()
{

}

score_plugin_media::~score_plugin_media()
{

}
