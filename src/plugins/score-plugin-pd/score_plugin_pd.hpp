#pragma once
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <QObject>

class score_plugin_pd final
    : public score::Plugin_QtInterface
    , public score::FactoryInterface_QtInterface
    , public score::CommandFactory_QtInterface
    , public score::ApplicationPlugin_QtInterface
{
public:
  score_plugin_pd();
  ~score_plugin_pd();

  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;

  // CommandFactory_QtInterface interface
  std::pair<const CommandGroupKey, CommandGeneratorMap>
  make_commands() override;

  score::Version version() const override;
  UuidKey<score::Plugin> key() const override;
  std::vector<score::PluginKey> required() const override;
};
