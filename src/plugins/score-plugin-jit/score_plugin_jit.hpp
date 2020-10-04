#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <QObject>

#include <utility>
#include <vector>

class score_plugin_jit final
    : public score::Plugin_QtInterface
    , public score::FactoryInterface_QtInterface
    , public score::CommandFactory_QtInterface
    , public score::ApplicationPlugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "3be955fe-4a09-498b-834a-25df3b7a8ca5")

public:
  score_plugin_jit();
  virtual ~score_plugin_jit() override;

private:
  // Defined in FactoryInterface_QtInterface
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& key) const override;

  // CommandFactory_QtInterface interface
  std::pair<const CommandGroupKey, CommandGeneratorMap>
  make_commands() override;

  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;

  std::vector<score::PluginKey> required() const override;
};
