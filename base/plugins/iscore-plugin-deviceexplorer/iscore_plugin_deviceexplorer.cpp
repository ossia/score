#include <Explorer/Panel/DeviceExplorerPanelFactory.hpp>

#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>

#include "iscore_plugin_deviceexplorer.hpp"
#include <iscore_plugin_deviceexplorer_commands_files.hpp>

#include <Device/Protocol/ProtocolList.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>
#include <iscore/tools/std/HashMap.hpp>

#include "DeviceExplorerApplicationPlugin.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPluginFactory.hpp>
#include <Explorer/Listening/ListeningHandlerFactoryList.hpp>

#include <iscore/serialization/AnySerialization.hpp>
#include <brigand/brigand.hpp>
namespace iscore
{

class InterfaceListBase;
class PanelFactory;
} // namespace iscore

iscore_plugin_deviceexplorer::iscore_plugin_deviceexplorer()
{
  QMetaType::registerComparators<UuidKey<Device::ProtocolFactory>>();
  qRegisterMetaType<Device::AddressSettings>();
  qRegisterMetaType<Device::FullAddressSettings>();
  qRegisterMetaType<Device::FullAddressAccessorSettings>();
  qRegisterMetaTypeStreamOperators<Device::AddressSettings>();
  qRegisterMetaTypeStreamOperators<Device::FullAddressSettings>();
  qRegisterMetaTypeStreamOperators<Device::FullAddressAccessorSettings>();

  auto& anySer = iscore::anySerializers();
  anySer.emplace(std::string("instanceBounds"), new iscore::any_serializer_t<ossia::net::instance_bounds>);
}

iscore_plugin_deviceexplorer::~iscore_plugin_deviceexplorer()
{
}

std::vector<std::unique_ptr<iscore::InterfaceListBase>>
iscore_plugin_deviceexplorer::factoryFamilies()
{
  return make_ptr_vector<iscore::InterfaceListBase, Device::ProtocolFactoryList, Explorer::ListeningHandlerFactoryList>();
}

std::vector<std::unique_ptr<iscore::InterfaceBase>>
iscore_plugin_deviceexplorer::factories(
    const iscore::ApplicationContext& ctx,
    const iscore::InterfaceKey& key) const
{
  return instantiate_factories<iscore::ApplicationContext, FW<iscore::DocumentPluginFactory, Explorer::DocumentPluginFactory>, FW<iscore::PanelDelegateFactory, Explorer::PanelDelegateFactory>>(
      ctx, key);
}

iscore::GUIApplicationContextPlugin*
iscore_plugin_deviceexplorer::make_applicationPlugin(
    const iscore::GUIApplicationContext& app)
{
  return new Explorer::ApplicationPlugin{app};
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
iscore_plugin_deviceexplorer::make_commands()
{
  using namespace Explorer::Command;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      DeviceExplorerCommandFactoryName(), CommandGeneratorMap{}};

  using Types = TypeList<
#include <iscore_plugin_deviceexplorer_commands.hpp>
      >;
  for_each_type<Types>(iscore::commands::FactoryInserter{cmds.second});

  return cmds;
}
