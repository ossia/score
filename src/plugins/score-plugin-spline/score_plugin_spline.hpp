#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/command/Command.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <utility>
#include <vector>
#include <verdigris>
/**
 * @namespace Automation
 * @brief Namespace related to the Spline process
 *
 * A 2D X-Y automation
 *
 */
class score_plugin_spline final : public score::Plugin_QtInterface,
                                  public score::FactoryInterface_QtInterface,
                                  public score::CommandFactory_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "cab4ef29-b641-4be0-83f8-5f90d0fcd575")

public:
  score_plugin_spline();
  ~score_plugin_spline() override;

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;

  std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands() override;
};
