#include "player.hpp"
#include <QCoreApplication>
#include <core/application/ApplicationSettings.hpp>
#include <core/application/SafeQApplication.hpp>
#include <core/document/DocumentBuilder.hpp>
#include <core/document/DocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationRegistrar.hpp>

#include <core/plugin/PluginManager.hpp>
#include <core/plugin/PluginDependencyGraph.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <iscore/model/ComponentSerialization.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <Process/ProcessList.hpp>
#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>

#include <ossia/editor/scenario/time_constraint.hpp>
#include <QPluginLoader>
#include <QJsonDocument>

#if defined(ISCORE_STATIC_PLUGINS)
  #include <iscore_static_plugins.hpp>
#endif

namespace iscore
{
class PlayerImpl :
    public QObject,
    public ApplicationInterface
{
  Q_OBJECT

public:
  PlayerImpl(int& argc, char** argv)
    : m_app{argc, argv}
  {
    m_instance = this;

    // Load global plug-in data
    ApplicationRegistrar reg(m_compData);
    reg.registerFactory(std::make_unique<DocumentDelegateList>());
    reg.registerFactory(std::make_unique<ValidityCheckerList>());
    reg.registerFactory(std::make_unique<SerializableComponentFactoryList>());
    reg.registerFactory(std::make_unique<DocumentPluginFactoryList>());
    reg.registerFactory(std::make_unique<SettingsDelegateFactoryList>());

    loadPlugins(reg, m_appContext);

    QSettings s;
    for (auto& plugin :
         m_appContext.interfaces<SettingsDelegateFactoryList>())
    {
      m_settings.push_back(plugin.makeModel(s, m_appContext));
    }

    connect(this, &PlayerImpl::sig_play, this, &PlayerImpl::play, Qt::QueuedConnection);
    connect(this, &PlayerImpl::sig_stop, this, &PlayerImpl::stop, Qt::QueuedConnection);
    connect(this, &PlayerImpl::sig_loadFile, this, &PlayerImpl::loadFile, Qt::QueuedConnection);
    connect(this, &PlayerImpl::sig_close, this, &PlayerImpl::close, Qt::QueuedConnection);
  }

  ~PlayerImpl()
  {
    closeDocument();
    close();
  }

  void closeDocument()
  {
    // Clear existing document
    if(m_currentDocument)
    {
      stop();

      m_execPlugin->clear();
      m_documents.documents().clear();
      m_documents.setCurrentDocument(nullptr);
      m_currentDocument.reset();
    }
  }

  void loadFile(QString file)
  {
    closeDocument();

    // Load new document
    QFile f(file);
    f.open(QIODevice::ReadOnly);

    const auto json = QJsonDocument::fromJson(f.readAll()).object();

    Scenario::ScenarioDocumentFactory fac;
    m_currentDocument = std::make_unique<Document>(json, fac, qApp);

    m_documents.documents().push_back(m_currentDocument.get());
    m_documents.setCurrentDocument(m_currentDocument.get());

    // Create execution plug-ins
    auto& ctx = m_currentDocument->context();
    m_localTreePlugin = new Engine::LocalTree::DocumentPlugin{ctx, Id<DocumentPlugin>{999}, nullptr};
    m_localTreePlugin->init();
    m_execPlugin = new Engine::Execution::DocumentPlugin{ctx, Id<DocumentPlugin>{998}, nullptr};

    DocumentModel& doc_model = m_currentDocument->model();
    doc_model.addPluginModel(m_localTreePlugin);
    doc_model.addPluginModel(m_execPlugin);
  }

  auto exec() { return m_app.exec(); }
  void close() { return m_app.exit(0); }

  void play()
  {
    DocumentModel& doc_model = m_currentDocument->model();
    Scenario::ConstraintModel& root_cst = safe_cast<Scenario::ScenarioDocumentModel&>(doc_model.modelDelegate()).baseConstraint();
    m_execPlugin->reload(root_cst);
    auto& exec_ctx = m_execPlugin->context();

    auto& exec_settings = m_appContext.settings<Engine::Execution::Settings::Model>();
    m_clock = exec_settings.makeClock(exec_ctx);

    m_clock->play(TimeVal::zero());
  }

  void stop()
  {
    if(m_clock)
      m_clock->stop();
    if(m_execPlugin)
      m_execPlugin->clear();
    m_clock.reset();
  }

  const ApplicationContext& context() const override
  {
    return m_appContext;
  }

  const ApplicationComponents& components() const override
  {
    return m_appContext.components;
  }

  void loadPlugins(
      ApplicationRegistrar& registrar, const ApplicationContext& context)
  {
    using namespace iscore;
    using namespace PluginLoader;
    // Here, the plug-ins that are effectively loaded.
    std::vector<Addon> availablePlugins;

    // Load static plug-ins
    for (QObject* plugin : QPluginLoader::staticInstances())
    {
      if (auto iscore_plug = dynamic_cast<Plugin_QtInterface*>(plugin))
      {
        Addon addon;
        addon.corePlugin = true;
        addon.plugin = iscore_plug;
        addon.key = iscore_plug->key();
        addon.corePlugin = true;
        availablePlugins.push_back(std::move(addon));
      }
    }

    loadPluginsInAllFolders(availablePlugins);
    loadAddonsInAllFolders(availablePlugins);

    registrar.registerAddons(availablePlugins);

    for (const Addon& addon : availablePlugins)
    {
      auto facfam_interface
          = dynamic_cast<FactoryList_QtInterface*>(addon.plugin);

      if (facfam_interface)
      {
        for (auto&& elt : facfam_interface->factoryFamilies())
        {
          registrar.registerFactory(std::move(elt));
        }
      }
    }

    PluginDependencyGraph graph{availablePlugins};
    const auto& add = graph.sortedAddons();
    if(!add.empty())
    {

      for(const Addon& addon : availablePlugins)
      {
        auto ctrl_plugin
            = dynamic_cast<ApplicationPlugin_QtInterface*>(addon.plugin);
        if (ctrl_plugin)
        {
          if(auto plug = ctrl_plugin->make_applicationPlugin(context))
            registrar.registerApplicationPlugin(plug);
        }
      }

      for (const Addon& addon : availablePlugins)
      {
        auto commands_plugin
            = dynamic_cast<CommandFactory_QtInterface*>(addon.plugin);
        if (commands_plugin)
        {
          registrar.registerCommands(commands_plugin->make_commands());
        }

        auto factories_plugin
            = dynamic_cast<FactoryInterface_QtInterface*>(addon.plugin);
        if (factories_plugin)
        {
          for (auto& factory_family : registrar.components().factories)
          {
            for (auto&& new_factory :
                 factories_plugin->factories(context, factory_family.first))
            {
              factory_family.second->insert(std::move(new_factory));
            }
          }
        }
      }
    }
  }

signals:
  void sig_play();
  void sig_stop();
  void sig_close();
  void sig_loadFile(QString);

private:
  // Load core application and plug-ins
  QCoreApplication m_app;

  // Application-specific
  ApplicationSettings m_globSettings;
  ApplicationComponentsData m_compData;

  ApplicationComponents m_components{m_compData};
  DocumentList m_documents;
  std::vector<std::unique_ptr<SettingsDelegateModel>> m_settings;
  ApplicationContext m_appContext{m_globSettings, m_components, m_documents, m_settings};

  // Document-specific
  std::unique_ptr<Document> m_currentDocument;
  Engine::Execution::DocumentPlugin* m_execPlugin{};
  Engine::LocalTree::DocumentPlugin* m_localTreePlugin{};
  std::unique_ptr<Engine::Execution::ClockManager> m_clock;
};

player::player(int& argc, char**& argv)
{
  m_thread = std::thread([&] {
    m_player = std::make_unique<PlayerImpl>(argc, argv);
    m_loaded = true;
    m_player->exec();
    m_player.reset();
  });
}

player::~player()
{
  if(m_player)
    m_player->sig_close();
  m_thread.join();
  m_player.reset();
}

void player::load(std::string path)
{
  while(!m_loaded);
  m_player->sig_loadFile(QString::fromStdString(path));
}

void player::play()
{
  assert(m_loaded);
  m_player->sig_play();
}

void player::stop()
{
  assert(m_loaded);
  m_player->sig_stop();
}
}

#include "player.moc"
