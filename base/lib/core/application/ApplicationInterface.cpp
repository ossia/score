// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationInterface.hpp"

#include <score/application/ApplicationContext.hpp>
#include <score/model/ComponentSerialization.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsFactory.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/model/ObjectRemover.hpp>

#include <core/application/ApplicationRegistrar.hpp>
#include <core/messages/MessagesPanel.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>
#include <core/settings/Settings.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
#include <core/undo/UndoApplicationPlugin.hpp>
#include <core/view/Window.hpp>

#include <QModelIndex>
namespace score
{
ApplicationInterface* ApplicationInterface::m_instance;
ApplicationInterface::~ApplicationInterface() = default;

ApplicationInterface::ApplicationInterface()
{
  qRegisterMetaType<QModelIndex>();
  qRegisterMetaType<ObjectIdentifierVector>("ObjectIdentifierVector");
  qRegisterMetaType<Selection>("Selection");
  qRegisterMetaType<Id<score::DocumentModel>>("Id<DocumentModel>");
  qRegisterMetaType<QVector<int>>();
  qRegisterMetaType<QPair<QString, QString>>();
  qRegisterMetaTypeStreamOperators<QPair<QString, QString>>();
}

ApplicationInterface& ApplicationInterface::instance()
{
  return *m_instance;
}

GUIApplicationInterface& GUIApplicationInterface::instance()
{
  return *static_cast<GUIApplicationInterface*>(
      ApplicationInterface::m_instance);
}

GUIApplicationInterface::~GUIApplicationInterface()
{
}

static void loadDefaultPlugins(
    const score::GUIApplicationContext& ctx,
    score::GUIApplicationRegistrar& r, score::Settings& settings,
    score::Presenter& presenter)
{
  using namespace score;
  r.registerFactory(std::make_unique<DocumentDelegateList>());
  r.registerFactory(std::make_unique<ValidityCheckerList>());
  r.registerFactory(std::make_unique<SerializableComponentFactoryList>());
  r.registerFactory(std::make_unique<ObjectRemoverList>());
  auto panels = std::make_unique<PanelDelegateFactoryList>();
  panels->insert(std::make_unique<UndoPanelDelegateFactory>());
  panels->insert(std::make_unique<MessagesPanelDelegateFactory>());
  r.registerFactory(std::move(panels));
  r.registerFactory(std::make_unique<DocumentPluginFactoryList>());
  r.registerFactory(std::make_unique<SettingsDelegateFactoryList>());

  r.registerGUIApplicationPlugin(new CoreApplicationPlugin{ctx, presenter});

  if (bool(presenter.view()))
    r.registerGUIApplicationPlugin(new UndoApplicationPlugin{ctx});
}

void GUIApplicationInterface::loadPluginData(
    const score::GUIApplicationContext& ctx,
    score::GUIApplicationRegistrar& registrar, score::Settings& settings,
    score::Presenter& presenter)
{
  loadDefaultPlugins(ctx, registrar, settings, presenter);

  score::PluginLoader::loadPlugins(registrar, ctx);

  // Now rehash our various hash tables
  presenter.optimize();

  // Load the settings
  QSettings s;
  for (auto& elt : ctx.interfaces<score::SettingsDelegateFactoryList>())
  {
    settings.setupSettingsPlugin(s, ctx, elt);
  }
  if (presenter.view())
  {
    presenter.setupGUI();
  }
  for (score::ApplicationPlugin* app_plug : ctx.applicationPlugins())
  {
    app_plug->initialize();
  }
  for (score::GUIApplicationPlugin* app_plug : ctx.guiApplicationPlugins())
  {
    app_plug->initialize();
  }

  if (presenter.view())
  {
    for (auto& panel_fac : ctx.interfaces<score::PanelDelegateFactoryList>())
    {
      registrar.registerPanel(panel_fac);
    }

    for (auto& panel : registrar.components().panels)
    {
      presenter.view()->setupPanel(panel.get());
    }
  }
}

GUIApplicationContext::GUIApplicationContext(
    const ApplicationSettings& a, const ApplicationComponents& b,
    DocumentManager& c, MenuManager& d, ToolbarManager& e, ActionManager& f,
    const std::vector<std::unique_ptr<SettingsDelegateModel>>& g,
    QMainWindow* mw)
    : score::ApplicationContext{a, b, c, g}
    , docManager{c}
    , menus{d}
    , toolbars{e}
    , actions{f}
    , mainWindow{mw}
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
