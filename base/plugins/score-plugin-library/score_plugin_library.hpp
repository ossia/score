#pragma once
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

class score_plugin_library : public QObject,
                              public score::Plugin_QtInterface,
                              public score::FactoryInterface_QtInterface
{
  Q_OBJECT
  Q_INTERFACES(score::Plugin_QtInterface score::FactoryInterface_QtInterface)
  SCORE_PLUGIN_METADATA(1, "f019a413-0ffd-417f-966a-a824548aca79")
public:
  score_plugin_library();
  virtual ~score_plugin_library();

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> guiFactories(
      const score::GUIApplicationContext&,
      const score::InterfaceKey& factoryName) const override;
};
