#pragma once
#include <QObject>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <utility>
#include <vector>

#include <score/application/ApplicationContext.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/command/Command.hpp>
#include <score/plugins/customfactory/FactoryInterface.hpp>

class score_plugin_js final : public QObject,
                               public score::Plugin_QtInterface,
                               public score::FactoryInterface_QtInterface,
                               public score::CommandFactory_QtInterface
{
  Q_OBJECT
  Q_INTERFACES(score::Plugin_QtInterface score::FactoryInterface_QtInterface
                   score::CommandFactory_QtInterface)
  SCORE_PLUGIN_METADATA(1, "0eb1db4b-a532-4961-ba1c-d9edbf08ef07")
public:
  score_plugin_js();
  virtual ~score_plugin_js();

private:
  // Process & inspector
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;

  // CommandFactory_QtInterface interface
  std::pair<const CommandGroupKey, CommandGeneratorMap>
  make_commands() override;
};
