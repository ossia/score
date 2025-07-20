#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/command/Command.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <utility>
#include <vector>

class score_plugin_clap final
    : public score::ApplicationPlugin_QtInterface
    , public score::FactoryInterface_QtInterface
    , public score::Plugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "05ba21fb-d429-455b-b6cf-c8068de1ded3")

public:
  score_plugin_clap();
  ~score_plugin_clap() override;

private:
  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;

  std::vector<score::InterfaceBase*> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& key) const override;

  std::vector<score::PluginKey> required() const override;
};
