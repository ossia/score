#include "score_plugin_gfx.hpp"

#include <Dataflow/WidgetInletFactory.hpp>
#include <Gfx/CameraDevice.hpp>
#include <Gfx/CommandFactory.hpp>
#include <Gfx/Filter/Executor.hpp>
#include <Gfx/Filter/Layer.hpp>
#include <Gfx/Filter/Library.hpp>
#include <Gfx/Filter/Process.hpp>
#include <Gfx/GeometryFilter/Executor.hpp>
#include <Gfx/GeometryFilter/Layer.hpp>
#include <Gfx/GeometryFilter/Library.hpp>
#include <Gfx/GeometryFilter/Process.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxDevice.hpp>
#include <Gfx/Images/Executor.hpp>
#include <Gfx/Images/ImageListChooser.hpp>
#include <Gfx/Images/Process.hpp>
#include <Gfx/Libav/LibavOutputDevice.hpp>
#include <Gfx/Settings/Factory.hpp>
#include <Gfx/SharedInputSettings.hpp>
#include <Gfx/SharedOutputSettings.hpp>
#include <Gfx/Text/Executor.hpp>
#include <Gfx/Text/Process.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/VSA/Executor.hpp>
#include <Gfx/VSA/Layer.hpp>
#include <Gfx/VSA/Library.hpp>
#include <Gfx/VSA/Process.hpp>
#include <Gfx/Video/Executor.hpp>
#include <Gfx/Video/Inspector.hpp>
#include <Gfx/Video/Layer.hpp>
#include <Gfx/Video/Process.hpp>
#include <Gfx/WindowDevice.hpp>

#include <score/plugins/FactorySetup.hpp>

#if defined(SCORE_HAS_SH4LT)
#include <Gfx/Sh4lt/Sh4ltInputDevice.hpp>
#include <Gfx/Sh4lt/Sh4ltOutputDevice.hpp>
#endif
#if defined(SCORE_HAS_SHMDATA)
#include <Gfx/Shmdata/ShmdataInputDevice.hpp>
#include <Gfx/Shmdata/ShmdataOutputDevice.hpp>
#endif
#if defined(HAS_SPOUT)
#include <Gfx/Spout/SpoutInput.hpp>
#include <Gfx/Spout/SpoutOutput.hpp>
#endif
#if defined(HAS_SYPHON)
#include <Gfx/Syphon/SyphonInput.hpp>
#include <Gfx/Syphon/SyphonOutput.hpp>
#endif
#if defined(HAS_FREENECT2)
#include <Gfx/Kinect2Device.hpp>
#endif
#include <QWindow>

#include <score_plugin_engine.hpp>
#include <score_plugin_gfx_commands_files.hpp>
score_plugin_gfx::score_plugin_gfx()
{
  qRegisterMetaType<Gfx::SharedInputSettings>();
  qRegisterMetaType<Gfx::SharedOutputSettings>();
  qRegisterMetaType<Gfx::CameraSettings>();
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
  qRegisterMetaType<Gfx::LibavOutputSettings>();
#endif

#if defined(HAS_FREENECT2)
  qRegisterMetaType<Gfx::Kinect2::Kinect2Settings>();
#endif
}

score_plugin_gfx::~score_plugin_gfx() { }

std::vector<score::InterfaceBase*> score_plugin_gfx::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Device::ProtocolFactory, Gfx::WindowProtocolFactory, Gfx::CameraProtocolFactory
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
         ,
         Gfx::LibavOutputProtocolFactory
#endif
#if defined(SCORE_HAS_SH4LT)
         ,
         Gfx::Sh4lt::InputFactory, Gfx::Sh4ltOutputProtocolFactory
#endif
#if defined(SCORE_HAS_SHMDATA)
         ,
         Gfx::Shmdata::InputFactory, Gfx::ShmdataOutputProtocolFactory
#endif
#if defined(HAS_SPOUT)
         ,
         Gfx::Spout::InputFactory, Gfx::SpoutProtocolFactory
#endif
#if defined(HAS_SYPHON)
         ,
         Gfx::Syphon::InputFactory, Gfx::SyphonProtocolFactory
#endif
#if defined(HAS_FREENECT2)
         ,
         Gfx::Kinect2::ProtocolFactory
#endif
         >,
      FW<Process::ProcessModelFactory, Gfx::Filter::ProcessFactory,
         Gfx::GeometryFilter::ProcessFactory, Gfx::Video::ProcessFactory,
         Gfx::Text::ProcessFactory, Gfx::Images::ProcessFactory,
         Gfx::VSA::ProcessFactory>,
      FW<Process::LayerFactory, Gfx::Filter::LayerFactory,
         Gfx::GeometryFilter::LayerFactory, Gfx::Video::LayerFactory,
         Gfx::VSA::LayerFactory>,
      FW<Execution::ProcessComponentFactory,
         Gfx::Filter::ProcessExecutorComponentFactory,
         Gfx::GeometryFilter::ProcessExecutorComponentFactory,
         Gfx::Video::ProcessExecutorComponentFactory,
         Gfx::Text::ProcessExecutorComponentFactory,
         Gfx::Images::ProcessExecutorComponentFactory,
         Gfx::VSA::ProcessExecutorComponentFactory>,
      FW<Inspector::InspectorWidgetFactory, Gfx::Video::InspectorFactory>,
      FW<Process::PortFactory,
         Dataflow::WidgetInletFactory<
             Gfx::Images::ImageListChooser, WidgetFactory::ImageListChooserItems>,
         Gfx::TextureInletFactory, Gfx::TextureOutletFactory, Gfx::GeometryInletFactory,
         Gfx::GeometryOutletFactory>,
      FW<Process::ProcessDropHandler, Gfx::Filter::DropHandler, Gfx::Video::DropHandler,
         Gfx::Images::DropHandler>,
      FW<Library::LibraryInterface, Gfx::Filter::LibraryHandler,
         Gfx::VSA::LibraryHandler, Gfx::Video::LibraryHandler,
         Gfx::Images::LibraryHandler, Gfx::GeometryFilter::LibraryHandler>,
      FW<score::SettingsDelegateFactory, Gfx::Settings::Factory>>(ctx, key);
}

score::GUIApplicationPlugin*
score_plugin_gfx::make_guiApplicationPlugin(const score::GUIApplicationContext& app)
{
  return new Gfx::ApplicationPlugin{app};
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_gfx::make_commands()
{
  using namespace Gfx;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_gfx_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}
auto score_plugin_gfx::required() const -> std::vector<score::PluginKey>
{
  return {score_plugin_engine::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_gfx)
