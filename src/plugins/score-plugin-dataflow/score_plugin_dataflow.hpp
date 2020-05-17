#pragma once
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

class score_plugin_dataflow final : public score::Plugin_QtInterface,
                                    public score::FactoryInterface_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "464b71fb-45d7-4278-8487-c85851769b34")

public:
  score_plugin_dataflow();
  ~score_plugin_dataflow() override;

private:
  std::vector<std::unique_ptr<score::InterfaceBase>>
  factories(const score::ApplicationContext& ctx, const score::InterfaceKey& key) const override;
};
