#pragma once
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <verdigris>

class score_plugin_transport
    : public score::Plugin_QtInterface
    , public score::FactoryInterface_QtInterface
    , public score::FactoryList_QtInterface
    , public score::ApplicationPlugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "9fd3ef99-fbfc-4002-917b-4c95135959d3")
public:
  score_plugin_transport();
  ~score_plugin_transport() override;

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> guiFactories(
      const score::GUIApplicationContext&,
      const score::InterfaceKey& factoryName) const override;
  std::vector<std::unique_ptr<score::InterfaceListBase>> factoryFamilies() override;
  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;
};
