#include "ApplicationInterface.hpp"
#include <core/application/ApplicationRegistrar.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>
#include <core/settings/Settings.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
#include <core/undo/UndoApplicationPlugin.hpp>
#include <core/view/View.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <iscore/model/ComponentSerialization.hpp>
namespace iscore
{
ApplicationInterface* ApplicationInterface::m_instance;
ApplicationInterface::~ApplicationInterface() = default;

ApplicationInterface::ApplicationInterface()
{
  qRegisterMetaType<ObjectIdentifierVector>("ObjectIdentifierVector");
  qRegisterMetaType<Selection>("Selection");
  qRegisterMetaType<Id<iscore::DocumentModel>>("Id<DocumentModel>");
  qRegisterMetaType<QVector<int>>();
}

ApplicationInterface& ApplicationInterface::instance()
{
  return *m_instance;
}

GUIApplicationInterface& GUIApplicationInterface::instance()
{
  return *static_cast<GUIApplicationInterface*>(ApplicationInterface::m_instance);
}

GUIApplicationInterface::~GUIApplicationInterface()
{
}
void GUIApplicationInterface::loadPluginData(
    const iscore::GUIApplicationContext& ctx,
    iscore::GUIApplicationRegistrar& registrar,
    iscore::Settings& settings,
    iscore::Presenter& presenter)
{
  registrar.registerFactory(std::make_unique<iscore::DocumentDelegateList>());
  registrar.registerFactory(std::make_unique<iscore::ValidityCheckerList>());
  registrar.registerFactory(std::make_unique<iscore::SerializableComponentFactoryList>());
  auto panels = std::make_unique<iscore::PanelDelegateFactoryList>();
  panels->insert(std::make_unique<iscore::UndoPanelDelegateFactory>());
  registrar.registerFactory(std::move(panels));
  registrar.registerFactory(
      std::make_unique<iscore::DocumentPluginFactoryList>());
  registrar.registerFactory(
      std::make_unique<iscore::SettingsDelegateFactoryList>());

  registrar.registerGUIApplicationPlugin(
      new iscore::CoreApplicationPlugin{ctx, presenter});
  registrar.registerGUIApplicationPlugin(
      new iscore::UndoApplicationPlugin{ctx});

  iscore::PluginLoader::loadPlugins(registrar, ctx);

  // Now rehash our various hash tables
  presenter.optimize();

  // Load the settings
  QSettings s;
  for (auto& elt :
       ctx.interfaces<iscore::SettingsDelegateFactoryList>())
  {
    settings.setupSettingsPlugin(s, ctx, elt);
  }

  presenter.setupGUI();

  for (iscore::GUIApplicationPlugin* app_plug :
       ctx.guiApplicationPlugins())
  {
    app_plug->initialize();
  }

  for (auto& panel_fac :
       ctx.interfaces<iscore::PanelDelegateFactoryList>())
  {
    registrar.registerPanel(panel_fac);
  }

  for(auto& panel : registrar.components().panels)
  {
    presenter.view()->setupPanel(panel.get());
  }
}

GUIApplicationContext::GUIApplicationContext(const ApplicationSettings& a, const ApplicationComponents& b, DocumentManager& c, MenuManager& d, ToolbarManager& e, ActionManager& f, const std::vector<std::unique_ptr<SettingsDelegateModel> >& g, QMainWindow& mw)
  : iscore::ApplicationContext{a, b, c, g},
    docManager{c},
    menus{d},
    toolbars{e},
    actions{f},
    mainWindow{mw}
{
}

ISCORE_LIB_BASE_EXPORT const ApplicationContext& AppContext()
{
  return ApplicationInterface::instance().context();
}

ISCORE_LIB_BASE_EXPORT const GUIApplicationContext& GUIAppContext()
{
  return GUIApplicationInterface::instance().context();
}

ISCORE_LIB_BASE_EXPORT const ApplicationComponents& AppComponents()
{
  return ApplicationInterface::instance().components();
}

}
