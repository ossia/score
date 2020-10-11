#include "score_plugin_media.hpp"

#include <Media/ApplicationPlugin.hpp>
#include <Media/Effect/Settings/Factory.hpp>
#include <Media/Inspector/Factory.hpp>
#include <Media/Merger/Executor.hpp>
#include <Media/Merger/Factory.hpp>
#include <Media/Merger/Inspector.hpp>
#include <Media/Metro/MetroExecutor.hpp>
#include <Media/Metro/MetroFactory.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>
#include <Media/Sound/SoundComponent.hpp>
#include <Media/Sound/SoundFactory.hpp>
#include <Media/Sound/SoundLibraryHandler.hpp>
#include <Media/Step/Executor.hpp>
#include <Media/Step/Factory.hpp>
#include <Media/Step/Inspector.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <Mixer/MixerPanel.hpp>
#include <wobjectimpl.h>
#if defined(HAS_LV2)
#include <Media/Effect/LV2/LV2EffectModel.hpp>
#include <Media/Effect/LV2/LV2Library.hpp>
#include <Media/Effect/LV2/LV2Window.hpp>
#endif
#if defined(HAS_VST2)
#include <Media/Effect/VST/VSTControl.hpp>
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Media/Effect/VST/VSTExecutor.hpp>
#include <Media/Effect/VST/VSTLibrary.hpp>
#include <Media/Effect/VST/VSTWidgets.hpp>
#endif
#if defined(HAS_FAUST)
#include <Media/Effect/Faust/FaustEffectModel.hpp>
#include <Media/Effect/Faust/FaustLibrary.hpp>
#endif
#include <Library/LibraryInterface.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <score_plugin_media_commands_files.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::Step::View)

#if __has_include(<libavcodec/avcodec.h>)
#define SCORE_HAS_LIBAV 1
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}
#endif

score_plugin_media::score_plugin_media()
{
#if SCORE_HAS_LIBAV
  av_register_all();
  avcodec_register_all();
  avdevice_register_all();
#endif

  qRegisterMetaType<Media::Sound::ComputedWaveform>();
  qRegisterMetaType<QVector<QImage>>();
  qRegisterMetaType<QVector<QImage*>>();
  qRegisterMetaType<ossia::audio_stretch_mode>();
  qRegisterMetaTypeStreamOperators<ossia::audio_stretch_mode>();
}

score_plugin_media::~score_plugin_media() { }

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_media::make_commands()
{
  using namespace Media;
#if defined(HAS_VST2)
  using namespace Media::VST;
#endif
#if defined(HAS_FAUST)
  using namespace Media::Faust;
#endif
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Media::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_media_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

score::ApplicationPlugin*
score_plugin_media::make_applicationPlugin(const score::ApplicationContext& app)
{
  return new Media::ApplicationPlugin{app};
}

score::GUIApplicationPlugin*
score_plugin_media::make_guiApplicationPlugin(const score::GUIApplicationContext& app)
{
  return new Media::GUIApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_media::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory,
         Media::Sound::ProcessFactory,
         Media::Step::ProcessFactory,
         Media::Metro::ProcessFactory,
         Media::Merger::ProcessFactory
#if defined(HAS_FAUST)
         ,
         Media::Faust::FaustEffectFactory
#endif
#if defined(HAS_LV2)
         ,
         Media::LV2::LV2EffectFactory
#endif
#if defined(HAS_VST2)
         ,
         Media::VST::VSTEffectFactory
#endif
         >,
      FW<Inspector::InspectorWidgetFactory,
         Media::Sound::InspectorFactory,
         Media::Step::InspectorFactory
         // , Media::Metro::InspectorFactory
         ,
         Media::Merger::InspectorFactory>,
      FW<Process::LayerFactory,
         Media::Sound::LayerFactory,
         Media::Metro::LayerFactory,
         Media::Step::LayerFactory,
         Media::Merger::LayerFactory
#if defined(HAS_VST2)
         ,
         Media::VST::LayerFactory
#endif
#if defined(HAS_LV2)
         ,
         Media::LV2::LayerFactory
#endif
#if defined(HAS_FAUST)
         ,
         Media::Faust::LayerFactory
#endif
         >,
      FW<Library::LibraryInterface
#if defined(HAS_VST2)
         ,
         Media::VST::LibraryHandler
#endif
#if defined(HAS_LV2)
         ,
         Media::LV2::LibraryHandler
#endif
#if defined(HAS_FAUST)
         ,
         Media::Faust::LibraryHandler
#endif
         ,
         Media::Sound::LibraryHandler>,

#if defined(HAS_VST2)
      FW<Process::PortFactory, Media::VST::VSTControlPortFactory>,
#endif

      FW<Execution::ProcessComponentFactory,
         Execution::SoundComponentFactory,
         Execution::StepComponentFactory,
         Execution::MetroComponentFactory,
         Execution::MergerComponentFactory
#if defined(HAS_VST2)
         ,
         Execution::VSTEffectComponentFactory
#endif
#if defined(HAS_LV2)
         ,
         Media::LV2::LV2EffectComponentFactory
#endif
#if defined(HAS_FAUST)
         ,
         Execution::FaustEffectComponentFactory
#endif
         >,
      FW<Process::ProcessDropHandler,
         Media::Sound::DropHandler
#if defined(HAS_FAUST)
         ,
         Media::Faust::DropHandler
#endif
         >,
      FW<score::SettingsDelegateFactory, Media::Settings::Factory>,
      FW<score::PanelDelegateFactory, Mixer::PanelDelegateFactory>
      >(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_media)
