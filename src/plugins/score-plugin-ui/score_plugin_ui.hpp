#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <verdigris>

class score_plugin_ui final : public score::FactoryInterface_QtInterface,
                              public score::Plugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "6946dfd3-72e7-4238-bba8-1229ee0cfe0b")
public:
  score_plugin_ui();
  ~score_plugin_ui() override;

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext&,
      const score::InterfaceKey& factoryName) const override;

  std::vector<score::PluginKey> required() const override;
};
