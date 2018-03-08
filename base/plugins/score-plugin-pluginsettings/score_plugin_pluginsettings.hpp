#pragma once
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

class score_plugin_pluginsettings
    : public QObject,
      public score::Plugin_QtInterface,
      public score::FactoryInterface_QtInterface
{
  Q_OBJECT
  Q_INTERFACES(score::Plugin_QtInterface score::FactoryInterface_QtInterface)

  SCORE_PLUGIN_METADATA(1, "f3407ffc-bb6a-494c-9a6e-d4f40028769e")
public:
  score_plugin_pluginsettings();
  virtual ~score_plugin_pluginsettings();

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;
};
