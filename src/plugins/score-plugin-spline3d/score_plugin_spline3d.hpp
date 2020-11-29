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
class score_plugin_spline3d final
    : public score::Plugin_QtInterface,
    public score::FactoryInterface_QtInterface,
    public score::CommandFactory_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "4296b41c-ab94-4165-8c1f-0418fa7fc2a4")

public:
  score_plugin_spline3d();
  ~score_plugin_spline3d() override;

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;
  std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands() override;
};
