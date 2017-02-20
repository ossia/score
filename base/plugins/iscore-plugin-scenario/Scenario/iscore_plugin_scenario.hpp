#pragma once
#include <QObject>
#include <QStringList>
#include <iscore/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <utility>
#include <vector>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/plugins/application/GUIApplicationPlugin.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
/*!
 * \namespace Scenario
 * \brief Main plug-in of i-score.
 *
 * This plug-in introduces the central logic of i-score :
 * a hierarchical model of objects in a timeline.
 *
 * It also has the core graphics logic of i-score based on QGraphicsScene
 * and QQuickWidget.
 *
 * As such, it is quite complex and provides a lot of classes.
 *
 * We have :
 * * The Scenario::ProcessModel process.
 * * The Scenario::PanelDelegate used to display elements.
 * * The Scenario::ScenarioDocumentModel, Scenario::ScenarioDocumentPresenter, Scenario::ScenarioDocumentView classes
 *   which set-up and displays the central part of an i-score document.
 * * The Scenario::ScenarioApplicationPlugin which handles tools, Action%s, and process focus.
 * * Model-view-presenter classes for the major i-score concepts :
 *   * Scenario::ConstraintModel,
 *   * Scenario::EventModel,
 *   * Scenario::TimeNodeModel,
 *   * Scenario::StateModel,
 *   * Scenario::RackModel,
 *   * Scenario::SlotModel
 *
 * * Scenario::BaseScenario is a minimalist, fixed scenario with a single constraint, a start state and an end state.
 * * Inspector elements for all these objects
 * * Scenario::Palette handles user input, movements, etc.
 *
 */
namespace iscore
{

class DocumentDelegateFactory;
class InterfaceListBase;
class PanelFactory;
} // namespace iscore

class iscore_plugin_scenario final
    : public QObject,
      public iscore::GUIApplicationPlugin_QtInterface,
      public iscore::CommandFactory_QtInterface,
      public iscore::FactoryList_QtInterface,
      public iscore::FactoryInterface_QtInterface,
      public iscore::Plugin_QtInterface
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID GUIApplicationPlugin_QtInterface_iid)
  Q_INTERFACES(
      iscore::GUIApplicationPlugin_QtInterface
          iscore::CommandFactory_QtInterface iscore::FactoryList_QtInterface
              iscore::FactoryInterface_QtInterface iscore::Plugin_QtInterface)

  ISCORE_PLUGIN_METADATA(1, "8439ef6c-90c3-4e08-8185-6a0f3c87f8b4")
public:
  iscore_plugin_scenario();
  virtual ~iscore_plugin_scenario();

private:
  // Application plugin interface
  iscore::GUIApplicationPlugin*
  make_applicationPlugin(const iscore::GUIApplicationContext& app) override;

  // NOTE : implementation is in CommandNames.cpp
  std::pair<const CommandGroupKey, CommandGeneratorMap>
  make_commands() override;

  // Offre la factory de Process
  std::vector<std::unique_ptr<iscore::InterfaceListBase>>
  factoryFamilies() override;

  // Crée les objets correspondant aux factories passées en argument.
  // ex. si QString = Process, renvoie un vecteur avec ScenarioFactory.
  std::vector<std::unique_ptr<iscore::InterfaceBase>> factories(
      const iscore::ApplicationContext&,
      const iscore::InterfaceKey& factoryName) const override;
};
