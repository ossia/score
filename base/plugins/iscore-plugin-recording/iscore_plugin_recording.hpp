#pragma once
#include <QObject>
#include <QStringList>
#include <iscore/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <utility>

#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/plugins/application/GUIApplicationPlugin.hpp>

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
      public iscore::ApplicationPlugin_QtInterface,
      public iscore::CommandFactory_QtInterface,
      public iscore::FactoryInterface_QtInterface
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID ApplicationPlugin_QtInterface_iid)
  Q_INTERFACES(iscore::Plugin_QtInterface
                   iscore::ApplicationPlugin_QtInterface
                       iscore::CommandFactory_QtInterface
                           iscore::FactoryInterface_QtInterface)

  ISCORE_PLUGIN_METADATA(1, "659ba25e-97e5-40d9-8db8-f7a8537035ad")
public:
  iscore_plugin_recording();
  virtual ~iscore_plugin_recording();

private:
  iscore::GUIApplicationPlugin*
  make_guiApplicationPlugin(const iscore::GUIApplicationContext& app) override;

  std::pair<const CommandGroupKey, CommandGeneratorMap>
  make_commands() override;

  std::vector<iscore::PluginKey> required() const override;
};
