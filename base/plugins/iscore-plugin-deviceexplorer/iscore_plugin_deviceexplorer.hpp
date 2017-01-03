#pragma once
#include <iscore/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <QObject>
#include <utility>
#include <vector>

#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/plugins/application/GUIApplicationPlugin.hpp>

namespace iscore
{

class InterfaceListBase;
class PanelFactory;
} // namespace iscore

class iscore_plugin_deviceexplorer final
    : public QObject,
      public iscore::Plugin_QtInterface,
      public iscore::FactoryList_QtInterface,
      public iscore::FactoryInterface_QtInterface,
      public iscore::GUIApplicationPlugin_QtInterface,
      public iscore::CommandFactory_QtInterface
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
  Q_INTERFACES(iscore::Plugin_QtInterface iscore::FactoryList_QtInterface
                   iscore::FactoryInterface_QtInterface
                       iscore::GUIApplicationPlugin_QtInterface
                           iscore::CommandFactory_QtInterface)

  ISCORE_PLUGIN_METADATA(1, "3c2a0e25-ab14-4c06-a1ba-033d721a520f")
public:
  iscore_plugin_deviceexplorer();
  virtual ~iscore_plugin_deviceexplorer();

private:
  // Factory for protocols
  std::vector<std::unique_ptr<iscore::InterfaceListBase>>
  factoryFamilies() override;

  std::vector<std::unique_ptr<iscore::InterfaceBase>> factories(
      const iscore::ApplicationContext& ctx,
      const iscore::InterfaceKey& factoryName) const override;

  // application plugin
  iscore::GUIApplicationPlugin*
  make_applicationPlugin(const iscore::GUIApplicationContext& app) override;

  std::pair<const CommandGroupKey, CommandGeneratorMap>
  make_commands() override;
};
