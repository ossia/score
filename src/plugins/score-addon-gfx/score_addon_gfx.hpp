#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/command/Command.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <utility>
#include <vector>

class score_addon_gfx final : public score::Plugin_QtInterface,
                              public score::FactoryInterface_QtInterface,
                              public score::CommandFactory_QtInterface,
                              public score::ApplicationPlugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "11f76f02-11a4-4803-858d-a744ccdc0a7e")

public:
  score_addon_gfx();
  ~score_addon_gfx() override;

private:
  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;

  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& key) const override;

  std::pair<const CommandGroupKey, CommandGeneratorMap>
  make_commands() override;
  std::vector<score::PluginKey> required() const override;
};
