#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <verdigris>

class score_plugin_fx final : public score::FactoryInterface_QtInterface,
                              public score::Plugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "bb86ea2a-cf2b-452c-90b4-ffcace8e6345")
public:
  score_plugin_fx();
  ~score_plugin_fx() override;

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext&,
      const score::InterfaceKey& factoryName) const override;

  std::vector<score::PluginKey> required() const override;
};
