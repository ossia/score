#pragma once
#include <QObject>
#include <QStringList>
#include <iscore/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <utility>

#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

namespace iscore
{

} // namespace iscore

/**
 * @brief The Recording class
 *
 * This plug-in is here to set-up things that require multiple plug-ins.
 * For instance, if a feature requires the Scenario, the Curve, and the
 * DeviceExplorer,
 * it should certainly be implemented here.
 *
 */
class iscore_plugin_recording final
    : public QObject,
      public iscore::Plugin_QtInterface,
      public iscore::GUIApplicationContextPlugin_QtInterface,
      public iscore::CommandFactory_QtInterface,
      public iscore::FactoryInterface_QtInterface
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID GUIApplicationContextPlugin_QtInterface_iid)
  Q_INTERFACES(iscore::Plugin_QtInterface
                   iscore::GUIApplicationContextPlugin_QtInterface
                       iscore::CommandFactory_QtInterface
                           iscore::FactoryInterface_QtInterface)

public:
  iscore_plugin_recording();
  virtual ~iscore_plugin_recording();

private:
  iscore::GUIApplicationContextPlugin*
  make_applicationPlugin(const iscore::GUIApplicationContext& app) override;

  std::vector<std::unique_ptr<iscore::InterfaceBase>> factories(
      const iscore::ApplicationContext& ctx,
      const iscore::InterfaceKey& key) const override;

  std::pair<const CommandGroupKey, CommandGeneratorMap>
  make_commands() override;

  QStringList required() const override;
  iscore::Version version() const override;
  UuidKey<iscore::Plugin> key() const override;
};
