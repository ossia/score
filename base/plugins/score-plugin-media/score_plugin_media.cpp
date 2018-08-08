#include "score_plugin_media.hpp"

#include <Media/ApplicationPlugin.hpp>
#include <Media/Effect/EffectExecutor.hpp>
#include <Media/Effect/EffectProcessFactory.hpp>
#include <Media/Effect/Inspector/EffectInspector.hpp>
#include <Media/Effect/Settings/Factory.hpp>
#include <Media/Inspector/Factory.hpp>
#include <Media/Merger/Executor.hpp>
#include <Media/Merger/Factory.hpp>
#include <Media/Merger/Inspector.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>
#include <Media/Sound/SoundComponent.hpp>
#include <Media/Sound/SoundFactory.hpp>
#include <Media/Step/Executor.hpp>
#include <Media/Step/Factory.hpp>
#include <Media/Step/Inspector.hpp>
#include <QAction>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#if defined(LILV_SHARED)
#  include <Media/Effect/LV2/LV2EffectModel.hpp>
#  include <Media/Effect/LV2/LV2Window.hpp>
#endif
#if defined(HAS_VST2)
#  include <Media/Effect/VST/VSTControl.hpp>
#  include <Media/Effect/VST/VSTEffectModel.hpp>
#  include <Media/Effect/VST/VSTExecutor.hpp>
#  include <Media/Effect/VST/VSTWidgets.hpp>
#endif
#if defined(HAS_FAUST)
#  include <Media/Effect/Faust/FaustEffectModel.hpp>
#endif
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/FactorySetup.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score_plugin_media_commands_files.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::Step::View)
namespace Media::VST
{
}

score_plugin_media::score_plugin_media()
{
}

score_plugin_media::~score_plugin_media()
{
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_media::make_commands()
{
  using namespace Media;
  using namespace Media::VST;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Media::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
    #include <score_plugin_media_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

score::ApplicationPlugin* score_plugin_media::make_applicationPlugin(
    const score::ApplicationContext& app)
{
  return new Media::ApplicationPlugin{app};
}

score::GUIApplicationPlugin* score_plugin_media::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  return new Media::GUIApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_media::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Media::Sound::ProcessFactory,
         Media::Effect::ProcessFactory, Media::Step::ProcessFactory, Media::Merger::ProcessFactory
#if defined(HAS_FAUST)
         , Media::Faust::FaustEffectFactory
#endif
#if defined(LILV_SHARED)
         , Media::LV2::LV2EffectFactory
#endif
#if defined(HAS_VST2)
         , Media::VST::VSTEffectFactory
#endif
         >,
      FW<Inspector::InspectorWidgetFactory
      , Media::Sound::InspectorFactory
      , Media::Effect::InspectorFactory
    #if defined(HAS_FAUST)
      , Media::Faust::InspectorFactory
    #endif
      , Media::Step::InspectorFactory
      , Media::Merger::InspectorFactory
      >,
      FW<Process::LayerFactory, Media::Sound::LayerFactory,
         Media::Effect::LayerFactory,
         Media::Step::LayerFactory, Media::Merger::LayerFactory
#if defined(HAS_VST2)
         , Media::VST::LayerFactory
#endif
#if defined(LILV_SHARED)
         ,
         Media::LV2::LayerFactory
#endif
#if defined(HAS_FAUST)
         ,
         Media::Faust::LayerFactory
#endif
         >,

#if defined(HAS_VST2)
      FW<Process::PortFactory, Media::VST::VSTControlPortFactory>,
#endif

      FW<Execution::ProcessComponentFactory,
         Execution::SoundComponentFactory,
         Media::EffectProcessComponentFactory,
         Execution::StepComponentFactory,
         Execution::MergerComponentFactory
#if defined(HAS_VST2)
         ,
         Execution::VSTEffectComponentFactory
#endif
#if defined(LILV_SHARED)
         ,
         Media::LV2::LV2EffectComponentFactory
#endif
#if defined(HAS_FAUST)
         ,
         Execution::FaustEffectComponentFactory
#endif
         >,
      FW<Scenario::DropHandler, Media::Sound::DropHandler>,
      FW<Scenario::IntervalDropHandler, Media::Sound::IntervalDropHandler>,
      FW<score::SettingsDelegateFactory, Media::Settings::Factory>>(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_media)
