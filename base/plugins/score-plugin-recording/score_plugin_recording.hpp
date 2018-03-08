#pragma once
#include <QObject>
#include <QStringList>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <utility>

#include <score/command/CommandGeneratorMap.hpp>
#include <score/command/Command.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>

namespace score
{

} // namespace score

/**
 * @brief The Recording class
 *
 * This plug-in is here to set-up things that require multiple plug-ins.
 * For instance, if a feature requires the Scenario, the Curve, and the
 * DeviceExplorer,
 * it should certainly be implemented here.
 *
 */
class score_plugin_recording final
    : public QObject,
      public score::Plugin_QtInterface,
      public score::ApplicationPlugin_QtInterface,
      public score::CommandFactory_QtInterface,
      public score::FactoryInterface_QtInterface
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID Plugin_QtInterface_iid)
  Q_INTERFACES(score::Plugin_QtInterface
                   score::ApplicationPlugin_QtInterface
                       score::CommandFactory_QtInterface
                           score::FactoryInterface_QtInterface)

  SCORE_PLUGIN_METADATA(1, "659ba25e-97e5-40d9-8db8-f7a8537035ad")
public:
  score_plugin_recording();
  virtual ~score_plugin_recording();

private:
  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;

  std::pair<const CommandGroupKey, CommandGeneratorMap>
  make_commands() override;

  std::vector<score::PluginKey> required() const override;
};
