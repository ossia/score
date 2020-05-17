#pragma once
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <verdigris>

class score_plugin_library : public score::Plugin_QtInterface,
                             public score::FactoryInterface_QtInterface,
                             public score::FactoryList_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "f019a413-0ffd-417f-966a-a824548aca79")
public:
  score_plugin_library();
  ~score_plugin_library() override;

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> guiFactories(
      const score::GUIApplicationContext&,
      const score::InterfaceKey& factoryName) const override;
  std::vector<std::unique_ptr<score::InterfaceListBase>> factoryFamilies() override;
};
