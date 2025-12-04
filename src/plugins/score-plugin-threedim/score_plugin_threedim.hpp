#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <verdigris>

class score_plugin_threedim final
    : public score::FactoryInterface_QtInterface
    , public score::Plugin_QtInterface
    , public score::CommandFactory_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "9f461313-af58-4365-a71f-b92fddc691cf")
public:
  score_plugin_threedim();
  ~score_plugin_threedim() override;

private:
  std::vector<score::InterfaceBase*> factories(
      const score::ApplicationContext&,
      const score::InterfaceKey& factoryName) const override;

  std::vector<score::PluginKey> required() const override;

  std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands() override;
};
