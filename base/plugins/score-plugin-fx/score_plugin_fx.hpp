#pragma once
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/customfactory/FactoryInterface.hpp>

class score_plugin_fx final
    : public QObject,
      public score::FactoryInterface_QtInterface,
      public score::Plugin_QtInterface
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
  Q_INTERFACES(score::FactoryInterface_QtInterface)

  SCORE_PLUGIN_METADATA(1, "bb86ea2a-cf2b-452c-90b4-ffcace8e6345")
public:
  score_plugin_fx();
  virtual ~score_plugin_fx();

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext&,
      const score::InterfaceKey& factoryName) const override;

  std::vector<score::PluginKey> required() const override;
};
