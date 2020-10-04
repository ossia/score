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

#include <QObject>

#include <utility>
#include <vector>

class score_plugin_nodal final : public score::Plugin_QtInterface,
                                public score::FactoryInterface_QtInterface,
                                public score::CommandFactory_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "8128b35f-369e-4309-b982-1ecf68203075")

public:
  score_plugin_nodal();
  ~score_plugin_nodal() override;

private:
  std::vector<std::unique_ptr<score::InterfaceBase>>
  factories(const score::ApplicationContext& ctx, const score::InterfaceKey& key) const override;

  std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands() override;
};
