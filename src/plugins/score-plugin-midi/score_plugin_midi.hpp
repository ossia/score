#pragma once
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <verdigris>

class score_plugin_midi final : public score::Plugin_QtInterface,
                                public score::FactoryInterface_QtInterface,
                                public score::CommandFactory_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "0a964c0f-dd69-4e5a-9577-0ec5695690b0")
public:
  score_plugin_midi();
  ~score_plugin_midi() override;

private:
  // Process & inspector
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;

  // CommandFactory_QtInterface interface
  std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands() override;
};
