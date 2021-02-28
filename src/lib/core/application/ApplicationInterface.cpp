// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationInterface.hpp"

#include <score/model/ComponentSerialization.hpp>
#include <score/model/ObjectRemover.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsFactory.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <core/application/ApplicationRegistrar.hpp>
#include <core/messages/MessagesPanel.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
#include <core/undo/UndoApplicationPlugin.hpp>
#include <core/view/Window.hpp>

#include <QGuiApplication>
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
  return *static_cast<GUIApplicationInterface*>(ApplicationInterface::m_instance);
}

GUIApplicationInterface::~GUIApplicationInterface() { }

static void loadDefaultPlugins(
    const score::GUIApplicationContext& ctx,
    score::GUIApplicationRegistrar& r,
    score::Settings& settings,
    score::Presenter& presenter)
{
  using namespace score;
  r.registerFactory(std::make_unique<DocumentDelegateList>());
  r.registerFactory(std::make_unique<ValidityCheckerList>());
#if defined(SCORE_SERIALIZABLE_COMPONENTS)
  r.registerFactory(std::make_unique<SerializableComponentFactoryList>());
#endif
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
    score::Settings& settings,
    score::Presenter& presenter)
{
  auto& ctx = presenter.applicationContext();
  score::GUIApplicationRegistrar registrar{
      presenter.components(),
      ctx,
      presenter.menuManager(),
      presenter.toolbarManager(),
      presenter.actionManager()};
  loadDefaultPlugins(ctx, registrar, settings, presenter);

  score::PluginLoader::loadPlugins(registrar, ctx);

  // Now rehash our various hash tables
  presenter.optimize();

  // Load the settings
#if defined(__EMSCRIPTEN__)
  // please don't look... currently (5.15) crash in
  // QSettings::QSettings so we disable settings and give a fake instance
  QSettings* ss = (QSettings*)alloca(sizeof(QSettings));
  memset(ss, 0, sizeof(QSettings));
  QSettings& s = *ss;
#else
  QSettings s;
#endif
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

    auto& panels = registrar.components().panels;
    std::sort(panels.begin(), panels.end(), [](const auto& lhs, const auto& rhs) {
      return lhs->defaultPanelStatus().priority < rhs->defaultPanelStatus().priority;
    });

    for (auto it = panels.rbegin(); it != panels.rend(); ++it)
    {
      presenter.view()->setupPanel((*it).get());
    }
    presenter.view()->allPanelsAdded();
  }
}

void GUIApplicationInterface::registerPlugin(Plugin_QtInterface& p)
{
  auto plugin = &p;
  auto presenter = qApp->findChild<score::Presenter*>();
  if (!presenter)
    return;
  auto& components = presenter->components();
  auto& context = this->context();

  score::GUIApplicationRegistrar registrar{
      presenter->components(),
      context,
      presenter->menuManager(),
      presenter->toolbarManager(),
      presenter->actionManager()};

  if (auto i = dynamic_cast<score::FactoryList_QtInterface*>(plugin))
  {
    for (auto&& elt : i->factoryFamilies())
    {
      registrar.registerFactory(std::move(elt));
    }
  }

  std::vector<score::ApplicationPlugin*> ap;
  std::vector<score::GUIApplicationPlugin*> gap;
  if (auto i = dynamic_cast<score::ApplicationPlugin_QtInterface*>(plugin))
  {
    if (auto plug = i->make_applicationPlugin(context))
    {
      ap.push_back(plug);
      registrar.registerApplicationPlugin(plug);
    }

    if (auto plug = i->make_guiApplicationPlugin(context))
    {
      gap.push_back(plug);
      registrar.registerGUIApplicationPlugin(plug);
    }
  }

  if (auto commands_plugin = dynamic_cast<score::CommandFactory_QtInterface*>(plugin))
  {
    registrar.registerCommands(commands_plugin->make_commands());
  }

  ossia::small_vector<score::InterfaceBase*, 8> settings_ifaces;
  ossia::small_vector<score::InterfaceBase*, 8> panels_ifaces;
  if (auto factories_plugin = dynamic_cast<score::FactoryInterface_QtInterface*>(plugin))
  {
    for (auto& factory_family : registrar.components().factories)
    {
      ossia::small_vector<score::InterfaceBase*, 8> ifaces;
      const score::ApplicationContext& base_ctx = context;
      // Register core factories
      for (auto&& new_factory : factories_plugin->factories(base_ctx, factory_family.first))
      {
        ifaces.push_back(new_factory.get());
        factory_family.second->insert(std::move(new_factory));
      }

      // Register GUI factories
      for (auto&& new_factory : factories_plugin->guiFactories(context, factory_family.first))
      {
        ifaces.push_back(new_factory.get());
        factory_family.second->insert(std::move(new_factory));
      }

      if (dynamic_cast<score::SettingsDelegateFactoryList*>(factory_family.second.get()))
      {
        settings_ifaces = std::move(ifaces);
      }
      else if (dynamic_cast<score::PanelDelegateFactoryList*>(factory_family.second.get()))
      {
        panels_ifaces = std::move(ifaces);
      }
    }
  }

  for (auto plug : ap)
    plug->initialize();
  for (auto plug : gap)
    plug->initialize();

#if defined(__EMSCRIPTEN__)
  // please don't look... currently (5.15) crash in
  // QSettings::QSettings so we disable settings and give a fake instance
  QSettings* ss = (QSettings*)alloca(sizeof(QSettings));
  memset(ss, 0, sizeof(QSettings));
  QSettings& s = *ss;
#else
  QSettings s;
#endif
  auto& settings = presenter->settings();
  for (auto& elt : settings_ifaces)
  {
    auto set = dynamic_cast<score::SettingsDelegateFactory*>(elt);
    SCORE_ASSERT(set);
    settings.setupSettingsPlugin(s, context, *set);
  }

  if (presenter->view())
  {
    for (auto& panel_fac : panels_ifaces)
    {
      auto p = static_cast<score::PanelDelegateFactory*>(panel_fac)->make(context);
      p->setModel(std::nullopt);
      components.panels.push_back(std::move(p));
      presenter->view()->setupPanel(components.panels.back().get());
    }
  }
}

void GUIApplicationInterface::requestExit()
{
  auto pres = qApp->findChild<score::Presenter*>();
  pres->exit();
}

void GUIApplicationInterface::forceExit()
{
  requestExit();
  QTimer::singleShot(500, [] { QCoreApplication::quit(); });
}

GUIApplicationContext::GUIApplicationContext(
    const ApplicationSettings& a,
    const ApplicationComponents& b,
    DocumentManager& c,
    MenuManager& d,
    ToolbarManager& e,
    ActionManager& f,
    const std::vector<std::unique_ptr<SettingsDelegateModel>>& g,
    QMainWindow* mw)
    : score::ApplicationContext{a, b, c, g}
    , docManager{c}
    , menus{d}
    , toolbars{e}
    , actions{f}
    , mainWindow{mw}
{
  if (auto win = qobject_cast<score::View*>(mw))
  {
    documentTabWidget = win->centralTabs;
  }
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
