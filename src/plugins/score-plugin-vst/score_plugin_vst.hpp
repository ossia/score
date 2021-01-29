#pragma once
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <verdigris>

class score_plugin_vst final : public score::Plugin_QtInterface,
                                 public score::FactoryInterface_QtInterface,
                                 public score::ApplicationPlugin_QtInterface,
                                 public score::CommandFactory_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "c3700a54-3dfe-4f9a-8e85-560810a178c1")

public:
  score_plugin_vst();
  ~score_plugin_vst() override;

  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;

  std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands() override;

  score::ApplicationPlugin* make_applicationPlugin(const score::ApplicationContext& app) override;
  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;
};
