#pragma once
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

class iscore_plugin_library : public QObject,
                              public iscore::Plugin_QtInterface,
                              public iscore::FactoryInterface_QtInterface
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
  Q_INTERFACES(iscore::Plugin_QtInterface iscore::FactoryInterface_QtInterface)
  ISCORE_PLUGIN_METADATA(1, "f019a413-0ffd-417f-966a-a824548aca79")
public:
  iscore_plugin_library();
  virtual ~iscore_plugin_library();

private:
  std::vector<std::unique_ptr<iscore::InterfaceBase>> factories(
      const iscore::ApplicationContext&,
      const iscore::InterfaceKey& factoryName) const override;
};
