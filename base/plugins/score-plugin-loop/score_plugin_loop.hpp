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

class score_plugin_loop final : public QObject,
                                 public score::Plugin_QtInterface,
                                 public score::FactoryInterface_QtInterface,
                                 public score::CommandFactory_QtInterface,
                                 public score::ApplicationPlugin_QtInterface
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID Plugin_QtInterface_iid)
  Q_INTERFACES(score::Plugin_QtInterface
               score::FactoryInterface_QtInterface
               score::CommandFactory_QtInterface
               score::ApplicationPlugin_QtInterface
               )
  SCORE_PLUGIN_METADATA(1, "db40e6eb-add3-4b6d-8957-13690aec290b")

public:
  score_plugin_loop();
  virtual ~score_plugin_loop();

private:
  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;

  // Process & inspector
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;

  // CommandFactory_QtInterface interface
  std::pair<const CommandGroupKey, CommandGeneratorMap>
  make_commands() override;
};
