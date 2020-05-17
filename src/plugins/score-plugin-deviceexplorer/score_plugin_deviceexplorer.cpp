// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_deviceexplorer.hpp"

#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/ApplicationPlugin.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPluginFactory.hpp>
#include <Explorer/Explorer/ExplorerModelProvider.hpp>
#include <Explorer/Listening/ListeningHandlerFactoryList.hpp>
#include <Explorer/Panel/DeviceExplorerPanelFactory.hpp>
#include <Explorer/Settings/ExplorerFactory.hpp>
#include <State/ValueSerialization.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/serialization/AnySerialization.hpp>
#include <score/tools/std/HashMap.hpp>

#include <ossia/audio/audio_protocol.hpp>

#include <score_plugin_deviceexplorer_commands_files.hpp>
struct audio_mapping_attr
{
  using type = ossia::audio_mapping;
  static constexpr auto text() { return "audio-mapping"; }
};
struct audio_kind_attr
{
  using type = std::string;
  static constexpr auto text() { return "audio-kind"; }
};
struct audio_channels_attr
{
  using type = int;
  static constexpr auto text() { return "audio-channels"; }
};
score_plugin_deviceexplorer::score_plugin_deviceexplorer()
{
#if defined(SCORE_STATIC_PLUGINS)
  Q_INIT_RESOURCE(DeviceExplorer);
#endif
  QMetaType::registerComparators<UuidKey<Device::ProtocolFactory>>();
  qRegisterMetaType<Device::AddressSettings>();
  qRegisterMetaType<Device::FullAddressSettings>();
  qRegisterMetaType<Device::FullAddressAccessorSettings>();
  qRegisterMetaTypeStreamOperators<Device::AddressSettings>();
  qRegisterMetaTypeStreamOperators<Device::FullAddressSettings>();
  qRegisterMetaTypeStreamOperators<Device::FullAddressAccessorSettings>();

  auto& anySer = score::anySerializers();
  ossia::for_each_in_tuple(
      std::tuple<
          ossia::net::instance_bounds_attribute,
          ossia::net::tags_attribute,
          ossia::net::description_attribute,
          ossia::net::refresh_rate_attribute,
          ossia::net::priority_attribute,
          ossia::net::value_step_size_attribute,
          ossia::net::extended_type_attribute,
          ossia::net::app_name_attribute,
          ossia::net::app_creator_attribute,
          ossia::net::app_version_attribute,
          ossia::net::hidden_attribute,
          ossia::net::default_value_attribute,
          audio_mapping_attr,
          audio_kind_attr,
          audio_channels_attr>(),
      [&](auto arg) {
        using type = decltype(arg);
        anySer.emplace(
            std::string(type::text()),
            std::make_unique<score::any_serializer_t<typename type::type>>());
      });
}

score_plugin_deviceexplorer::~score_plugin_deviceexplorer() { }

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_deviceexplorer::factoryFamilies()
{
  return make_ptr_vector<
      score::InterfaceListBase,
      Device::ProtocolFactoryList,
      Device::DeviceModelProviderList,
      Explorer::ListeningHandlerFactoryList>();
}

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_deviceexplorer::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<score::DocumentPluginFactory,
         Explorer::DocumentPluginFactory,
         Explorer::ProjectSettings::Factory>,
      FW<score::PanelDelegateFactory, Explorer::PanelDelegateFactory>,
      FW<score::SettingsDelegateFactory, Explorer::Settings::Factory>,
      FW<Device::DeviceModelProvider, Explorer::ModelProvider>>(ctx, key);
}

score::GUIApplicationPlugin*
score_plugin_deviceexplorer::make_guiApplicationPlugin(const score::GUIApplicationContext& app)
{
  return new Explorer::ApplicationPlugin{app};
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_deviceexplorer::make_commands()
{
  using namespace Explorer::Command;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      DeviceExplorerCommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_deviceexplorer_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_deviceexplorer)
