// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationInterface.hpp"
#include <core/application/ApplicationRegistrar.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>
#include <core/settings/Settings.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
#include <core/undo/UndoApplicationPlugin.hpp>
#include <core/view/View.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/model/ComponentSerialization.hpp>
namespace score
{
ApplicationInterface* ApplicationInterface::m_instance;
ApplicationInterface::~ApplicationInterface() = default;

ApplicationInterface::ApplicationInterface()
{
  qRegisterMetaType<ObjectIdentifierVector>("ObjectIdentifierVector");
  qRegisterMetaType<Selection>("Selection");
  qRegisterMetaType<Id<score::DocumentModel>>("Id<DocumentModel>");
  qRegisterMetaType<QVector<int>>();
  qRegisterMetaType<QPair<QString,QString>>();
  qRegisterMetaTypeStreamOperators<QPair<QString,QString>>();
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
    const score::GUIApplicationContext& ctx,
    score::GUIApplicationRegistrar& registrar,
    score::Settings& settings,
    score::Presenter& presenter)
{
  registrar.registerFactory(std::make_unique<score::DocumentDelegateList>());
  registrar.registerFactory(std::make_unique<score::ValidityCheckerList>());
  registrar.registerFactory(std::make_unique<score::SerializableComponentFactoryList>());
  auto panels = std::make_unique<score::PanelDelegateFactoryList>();
  panels->insert(std::make_unique<score::UndoPanelDelegateFactory>());
  registrar.registerFactory(std::move(panels));
  registrar.registerFactory(
      std::make_unique<score::DocumentPluginFactoryList>());
  registrar.registerFactory(
      std::make_unique<score::SettingsDelegateFactoryList>());

  registrar.registerGUIApplicationPlugin(
      new score::CoreApplicationPlugin{ctx, presenter});

  if(presenter.view())
  {
    registrar.registerGUIApplicationPlugin(
          new score::UndoApplicationPlugin{ctx});
  }
  score::PluginLoader::loadPlugins(registrar, ctx);

  // Now rehash our various hash tables
  presenter.optimize();

  // Load the settings
  QSettings s;
  for (auto& elt :
       ctx.interfaces<score::SettingsDelegateFactoryList>())
  {
    settings.setupSettingsPlugin(s, ctx, elt);
  }

  if(presenter.view())
  {
    presenter.setupGUI();
  }

  for (score::ApplicationPlugin* app_plug :
       ctx.applicationPlugins())
  {
    app_plug->initialize();
  }

  if(presenter.view())
  {
    for (auto& panel_fac :
         ctx.interfaces<score::PanelDelegateFactoryList>())
    {
      registrar.registerPanel(panel_fac);
    }

    for(auto& panel : registrar.components().panels)
    {
      presenter.view()->setupPanel(panel.get());
    }
  }
}

GUIApplicationContext::GUIApplicationContext(const ApplicationSettings& a, const ApplicationComponents& b, DocumentManager& c, MenuManager& d, ToolbarManager& e, ActionManager& f, const std::vector<std::unique_ptr<SettingsDelegateModel> >& g, QMainWindow* mw)
  : score::ApplicationContext{a, b, c, g},
    docManager{c},
    menus{d},
    toolbars{e},
    actions{f},
    mainWindow{mw}
{
}

SCORE_LIB_BASE_EXPORT const ApplicationContext& AppContext()
{
  return ApplicationInterface::instance().context();
}

SCORE_LIB_BASE_EXPORT const GUIApplicationContext& GUIAppContext()
{
  return GUIApplicationInterface::instance().context();
}

SCORE_LIB_BASE_EXPORT const ApplicationComponents& AppComponents()
{
  return ApplicationInterface::instance().components();
}

}
