#include "player.hpp"
#include "player_impl.hpp"
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/detail/logger.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>

#if defined(ISCORE_STATIC_PLUGINS)
  #include <iscore_static_plugins.hpp>
#endif

namespace iscore
{

PlayerImpl::PlayerImpl()
{
  m_instance = this;
  init();
}

PlayerImpl::PlayerImpl(bool)
  : argv{new char*[1]{new char[16]{'\0'}}}
  , m_app{std::make_unique<QCoreApplication>(argc, argv)}
{
  m_instance = this;
  init();
}

PlayerImpl::~PlayerImpl()
{
  if(argv)
  {
    delete[] argv[0];
    delete[] argv;
  }
  closeDocument();
  close();
}

void PlayerImpl::init()
{
  ossia::context c;
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
  connect(this, &PlayerImpl::sig_registerDevice, this, &PlayerImpl::registerDevice, Qt::QueuedConnection);
}

void PlayerImpl::closeDocument()
{
  // Clear existing document
  if(m_currentDocument)
  {
    stop();

    m_execPlugin->clear();

    while(!m_ownedDevices.empty())
      unregisterDevice(m_ownedDevices.back());

    m_execPlugin = nullptr;
    m_localTreePlugin = nullptr;
    m_devicesPlugin = nullptr;
    m_documents.documents().clear();
    m_documents.setCurrentDocument(nullptr);
    m_currentDocument.reset();
  }
}

void PlayerImpl::loadFile(QString file)
{
  closeDocument();

  // Load new document
  QFile f(file);
  f.open(QIODevice::ReadOnly);

  const auto json = QJsonDocument::fromJson(f.readAll()).object();

  Scenario::ScenarioDocumentFactory fac;
  m_currentDocument = std::make_unique<Document>(json, fac, QCoreApplication::instance());

  m_documents.documents().push_back(m_currentDocument.get());
  m_documents.setCurrentDocument(m_currentDocument.get());

  // Create execution plug-ins
  const iscore::DocumentContext& ctx = m_currentDocument->context();
  m_localTreePlugin = new Engine::LocalTree::DocumentPlugin{ctx, Id<DocumentPlugin>{999}, nullptr};
  m_localTreePlugin->init();
  m_execPlugin = new Engine::Execution::DocumentPlugin{ctx, Id<DocumentPlugin>{998}, nullptr};

  DocumentModel& doc_model = m_currentDocument->model();
  doc_model.addPluginModel(m_localTreePlugin);
  doc_model.addPluginModel(m_execPlugin);

  m_devicesPlugin = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
}

void PlayerImpl::registerDevice(ossia::net::device_base* dev)
{
  ISCORE_ASSERT(m_devicesPlugin);
  Device::DeviceInterface* d = m_devicesPlugin->list().findDevice(QString::fromStdString(dev->get_name()));

  if(auto sd = static_cast<Engine::Network::OwningOSSIADevice*>(d))
  {
    sd->replaceDevice(dev);
    m_ownedDevices.push_back(dev);
  }
  else
  {
    m_devicesPlugin->list().apply([] (const Device::DeviceInterface& d) { qDebug() << d.settings().name; } );
    ossia::logger().error("Tried to register unknown device: {}", dev->get_name());
  }
}

void PlayerImpl::unregisterDevice(ossia::net::device_base* dev)
{

  ISCORE_ASSERT(m_devicesPlugin);
  Device::DeviceInterface* d = m_devicesPlugin->list().findDevice(QString::fromStdString(dev->get_name()));
  if(auto sd = static_cast<Engine::Network::OwningOSSIADevice*>(d))
  {
    sd->releaseDevice();
    m_ownedDevices.erase(ossia::find(m_ownedDevices, dev));
  }
}

void PlayerImpl::exec() { if(m_app) m_app->exec(); }

void PlayerImpl::close() { if(m_app) m_app->exit(0); }

void PlayerImpl::play()
{
  DocumentModel& doc_model = m_currentDocument->model();
  Scenario::ConstraintModel& root_cst = safe_cast<Scenario::ScenarioDocumentModel&>(doc_model.modelDelegate()).baseConstraint();
  m_execPlugin->reload(root_cst);
  auto& exec_ctx = m_execPlugin->context();

  auto& exec_settings = m_appContext.settings<Engine::Execution::Settings::Model>();
  m_clock = exec_settings.makeClock(exec_ctx);

  m_clock->play(TimeVal::zero());
}

void PlayerImpl::stop()
{
  if(m_clock)
    m_clock->stop();
  if(m_execPlugin)
    m_execPlugin->clear();
  m_clock.reset();
}

const ApplicationContext&PlayerImpl::context() const
{
  return m_appContext;
}

const ApplicationComponents&PlayerImpl::components() const
{
  return m_appContext.components;
}

void PlayerImpl::loadPlugins(ApplicationRegistrar& registrar, const ApplicationContext& context)
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




Player::Player()
{
  if(QCoreApplication::instance())
  { // we run in the main thread
    m_player = std::make_unique<PlayerImpl>();
    m_loaded = true;
  }
  else
  {
    m_thread = std::thread([&] {
      m_player = std::make_unique<PlayerImpl>(true);
      m_loaded = true;
      m_player->exec();
      m_player.reset();
    });
  }
}

Player::~Player()
{
  if(m_player)
    m_player->sig_close();

  if(m_thread.joinable())
    m_thread.join();

  m_player.reset();
}

void Player::load(std::string path)
{
  while(!m_loaded);
  m_player->sig_loadFile(QString::fromStdString(path));
}

void Player::play()
{
  assert(m_loaded);
  m_player->sig_play();
}

void Player::stop()
{
  assert(m_loaded);
  m_player->sig_stop();
}

void Player::registerDevice(ossia::net::device_base& dev)
{
  m_player->sig_registerDevice(&dev);
}

}
