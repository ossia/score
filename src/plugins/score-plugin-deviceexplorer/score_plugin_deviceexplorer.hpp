#pragma once
#include <score/command/Command.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <utility>
#include <vector>
#include <verdigris>

namespace score
{

class InterfaceListBase;
class PanelFactory;
} // namespace score

class score_plugin_deviceexplorer final : public score::Plugin_QtInterface,
                                          public score::FactoryList_QtInterface,
                                          public score::FactoryInterface_QtInterface,
                                          public score::ApplicationPlugin_QtInterface,
                                          public score::CommandFactory_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "3c2a0e25-ab14-4c06-a1ba-033d721a520f")
public:
  score_plugin_deviceexplorer();
  ~score_plugin_deviceexplorer() override;

private:
  // Factory for protocols
  std::vector<std::unique_ptr<score::InterfaceListBase>> factoryFamilies() override;

  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;

  // application plugin
  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;

  std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands() override;
};
