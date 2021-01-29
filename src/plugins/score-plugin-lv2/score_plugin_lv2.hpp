#pragma once
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <verdigris>

class score_plugin_lv2 final : public score::Plugin_QtInterface,
                                 public score::FactoryInterface_QtInterface,
                                 public score::ApplicationPlugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "18080d64-951d-407f-80b6-b897b8cde423")

public:
  score_plugin_lv2();
  ~score_plugin_lv2() override;

  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;

  score::ApplicationPlugin* make_applicationPlugin(const score::ApplicationContext& app) override;
};
