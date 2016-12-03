#include "ApplicationInterface.hpp"
#include <core/application/ApplicationRegistrar.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>
#include <core/settings/Settings.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
#include <core/undo/UndoApplicationPlugin.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
namespace iscore
{
ApplicationInterface* ApplicationInterface::m_instance;
ApplicationInterface::~ApplicationInterface() = default;

ApplicationInterface::ApplicationInterface()
{
  qRegisterMetaType<ObjectIdentifierVector>("ObjectIdentifierVector");
  qRegisterMetaType<Selection>("Selection");
  qRegisterMetaType<Id<iscore::DocumentModel>>("Id<DocumentModel>");
}

ApplicationInterface& ApplicationInterface::instance()
{
  return *m_instance;
}

void ApplicationInterface::loadPluginData(
    const iscore::GUIApplicationContext& ctx,
    iscore::ApplicationRegistrar& registrar,
    iscore::Settings& settings,
    iscore::Presenter& presenter)
{
  registrar.registerFactory(std::make_unique<iscore::DocumentDelegateList>());
  registrar.registerFactory(std::make_unique<iscore::ValidityCheckerList>());
  auto panels = std::make_unique<iscore::PanelDelegateFactoryList>();
  panels->insert(std::make_unique<iscore::UndoPanelDelegateFactory>());
  registrar.registerFactory(std::move(panels));
  registrar.registerFactory(
      std::make_unique<iscore::DocumentPluginFactoryList>());
  registrar.registerFactory(
      std::make_unique<iscore::SettingsDelegateFactoryList>());

  registrar.registerApplicationContextPlugin(
      new iscore::CoreApplicationPlugin{ctx, presenter});
  registrar.registerApplicationContextPlugin(
      new iscore::UndoApplicationPlugin{ctx});

  iscore::PluginLoader::loadPlugins(registrar, ctx);

  // Now rehash our various hash tables
  presenter.optimize();

  // Load the settings
  QSettings s;
  for (auto& elt :
       ctx.components.factory<iscore::SettingsDelegateFactoryList>())
  {
    settings.setupSettingsPlugin(s, ctx, elt);
  }

  presenter.setupGUI();

  for (iscore::GUIApplicationContextPlugin* app_plug :
       ctx.components.applicationPlugins())
  {
    app_plug->initialize();
  }

  for (auto& panel_fac :
       context().components.factory<iscore::PanelDelegateFactoryList>())
  {
    registrar.registerPanel(panel_fac);
  }
}

ISCORE_LIB_BASE_EXPORT const ApplicationContext& AppContext()
{
  return ApplicationInterface::instance().context();
}

ISCORE_LIB_BASE_EXPORT const ApplicationComponents& AppComponents()
{
  return ApplicationInterface::instance().components();
}
}
