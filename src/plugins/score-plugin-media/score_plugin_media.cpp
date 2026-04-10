#include "score_plugin_media.hpp"

#include <Process/Dataflow/WidgetInlets.hpp>

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <Dataflow/WidgetInletFactory.hpp>
#include <Library/LibraryInterface.hpp>
#include <Media/AudioFileChooserWidget.hpp>
#include <Media/Effect/Settings/Factory.hpp>
#include <Media/Inspector/Factory.hpp>
#include <Media/Libav.hpp>
#include <Media/Merger/Executor.hpp>
#include <Media/Merger/Factory.hpp>
#include <Media/Merger/Inspector.hpp>
#include <Media/Metro/MetroExecutor.hpp>
#include <Media/Metro/MetroFactory.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>
#include <Media/Sound/BW64/Bw64Drop.hpp>
#include <Media/Sound/QImagePool.hpp>
#include <Media/Sound/SoundComponent.hpp>
#include <Media/Sound/SoundFactory.hpp>
#include <Media/Sound/SoundLibraryHandler.hpp>
#include <Media/Step/Executor.hpp>
#include <Media/Step/Factory.hpp>
#include <Media/Step/Inspector.hpp>
#include <Mixer/MixerPanel.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <score_plugin_media_commands_files.hpp>
#include <wobjectimpl.h>

#if SCORE_HAS_LIBAV
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}
#endif

score_plugin_media::score_plugin_media()
{
#if SCORE_HAS_LIBAV
#if LIBAVCODEC_VERSION_MAJOR < 59
  av_register_all();
  avcodec_register_all();
#endif
  avdevice_register_all();

  if((!qEnvironmentVariableIsSet("SCORE_SKIP_FFMPEG_CHECK"))
     && ((LIBAVFORMAT_VERSION_INT != avformat_version())
         || (LIBAVCODEC_VERSION_INT != avcodec_version())
         || (LIBAVUTIL_VERSION_INT != avutil_version())
         || (LIBAVDEVICE_VERSION_INT != avdevice_version())))
  {

    qFatal(
        "Run-time FFMPEG libraries are different than the one score is built against, "
        "this will cause crashes. Aborting.\n\nTo force running despite the likely "
        "crashes, set SCORE_SKIP_FFMPEG_CHECK=1 environment variable.");
  }
#endif

  qRegisterMetaType<Media::Sound::ComputedWaveform>();
  qRegisterMetaType<QVector<QImage>>();
  qRegisterMetaType<QVector<QImage*>>();
  qRegisterMetaType<ossia::audio_stretch_mode>();

  QObject::connect(
      QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
      QCoreApplication::instance(), [] { Media::Sound::QImagePool::instance().gc(); });
}

score_plugin_media::~score_plugin_media() { }

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_media::make_commands()
{
  using namespace Media;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Media::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_media_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_media::factoryFamilies()
{
  return make_ptr_vector<
      score::InterfaceListBase, Media::Settings::PluginSettingsFactoryList>();
}

std::vector<score::InterfaceBase*> score_plugin_media::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Media::Sound::ProcessFactory,
         Media::Step::ProcessFactory, Media::Metro::ProcessFactory,
         Media::Merger::ProcessFactory>,
      FW<Inspector::InspectorWidgetFactory, Media::Sound::InspectorFactory,
         Media::Step::InspectorFactory
         // , Media::Metro::InspectorFactory
         ,
         Media::Merger::InspectorFactory>,
      FW<Process::LayerFactory, Media::Sound::LayerFactory, Media::Metro::LayerFactory,
         Media::Step::LayerFactory>,
      FW<Library::LibraryInterface, Media::Sound::LibraryHandler>,

      FW<Execution::ProcessComponentFactory, Execution::SoundComponentFactory,
         Execution::StepComponentFactory, Execution::MetroComponentFactory,
         Execution::MergerComponentFactory>,
      FW<Process::ProcessDropHandler, Media::Sound::DropHandler
#if defined(SCORE_HAS_BW64_ADM)
         , Media::BW64::DropHandler
#endif
         >,
      FW<score::SettingsDelegateFactory, Media::Settings::Factory>,
      FW<score::PanelDelegateFactory, Mixer::PanelDelegateFactory>,
      FW<Process::PortFactory, Dataflow::WidgetInletFactory<
                                   Process::AudioFileChooser, Media::AudioFileChooser>>

      >(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_media)
