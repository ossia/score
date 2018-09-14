#pragma once
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <wobjectdefs.h>

class score_plugin_pluginsettings : public score::Plugin_QtInterface,
                                    public score::FactoryInterface_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "f3407ffc-bb6a-494c-9a6e-d4f40028769e")
public:
  score_plugin_pluginsettings();
  ~score_plugin_pluginsettings() override;

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;
};
