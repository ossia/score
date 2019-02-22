// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "player.hpp"

#include "player_impl.hpp"

#include <Device/Protocol/DeviceInterface.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <ossia/detail/logger.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <Execution/Settings/ExecutorModel.hpp>
#if defined(SCORE_PLUGIN_AUDIO)
#include <Audio/AudioStreamEngine/AudioApplicationPlugin.hpp>
#include <Audio/AudioStreamEngine/AudioDocumentPlugin.hpp>
#include <Audio/AudioStreamEngine/Clock/AudioClock.hpp>
#include <Audio/Settings/Card/CardSettingsModel.hpp>
#endif

#if defined(SCORE_ADDON_NETWORK)
#include <Network/Document/ClientPolicy.hpp>
#include <Network/Document/DocumentPlugin.hpp>
#include <Network/Document/Execution/BasicPruner.hpp>
#include <Network/PlayerPlugin.hpp>
#include <Network/Settings/NetworkSettingsModel.hpp>
#endif

#if defined(SCORE_STATIC_PLUGINS)
#include <score_static_plugins.hpp>
#endif

namespace score
{

PlayerImpl::PlayerImpl()
{
  m_instance = this;
}

PlayerImpl::PlayerImpl(bool)
    : argv{new char* [1] {
      new char[16]
      {
        '\0'
      }
    }}
    , m_app{std::make_unique<QCoreApplication>(argc, argv)}
{
  m_instance = this;
}

PlayerImpl::~PlayerImpl()
{
  if (argv)
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

  QSettings s("OSSIA", "score");
  for (auto& plugin : m_appContext.interfaces<SettingsDelegateFactoryList>())
  {
    m_settings.push_back(plugin.makeModel(s, m_appContext));
  }

#if defined(SCORE_ADDON_NETWORK)
  auto& ns = m_appContext.settings<Network::Settings::Model>();
  srand(time(NULL));
  ns.setClientName(
      QString::fromStdString(fmt::format("player.{}", rand() % 100)));
#endif

  for (score::ApplicationPlugin* app_plug : m_components.applicationPlugins())
  {
    app_plug->initialize();
  }

#if defined(SCORE_PLUGIN_AUDIO)
  auto& exec_settings = m_appContext.settings<Execution::Settings::Model>();
  exec_settings.setClock(
      Audio::AudioStreamEngine::AudioClockFactory::static_concreteKey());
  auto& audio_settings = m_appContext.settings<Audio::Settings::Model>();
  audio_settings.setDriver("PortAudio");
#endif

#if defined(SCORE_ADDON_NETWORK)
  auto& netplug = m_components.applicationPlugin<Network::PlayerPlugin>();
  netplug.documentLoader = [&](const QByteArray& arr) {
    loadArray(arr);
    return m_currentDocument.get();
  };
  netplug.onDocumentLoaded = [&] {
    Document& doc = *m_currentDocument;
    auto plug = doc.context().findPlugin<Network::NetworkDocumentPlugin>();
    if (plug)
    {
      m_networkPlugin = plug;
      auto pol
          = dynamic_cast<Network::PlayerClientEditionPolicy*>(&plug->policy());
      if (pol)
      {
        pol->onPlay = [this] {
          prepare_play();

          auto& exec_ctx = m_execPlugin->context();
          m_execPlugin->runAllCommands();
          Network::BasicPruner{*m_networkPlugin}(exec_ctx);
          m_execPlugin->runAllCommands();

          do_play();
        };
        pol->onStop = [this] { stop(); };
      }
    }
  };

#endif
  connect(
      this, &PlayerImpl::sig_play, this, &PlayerImpl::play,
      Qt::QueuedConnection);
  connect(
      this, &PlayerImpl::sig_stop, this, &PlayerImpl::stop,
      Qt::QueuedConnection);
  connect(
      this, &PlayerImpl::sig_loadFile, this, &PlayerImpl::loadFile,
      Qt::QueuedConnection);
  connect(
      this, &PlayerImpl::sig_close, this, &PlayerImpl::close,
      Qt::QueuedConnection);
  connect(
      this, &PlayerImpl::sig_registerDevice, this, &PlayerImpl::registerDevice,
      Qt::QueuedConnection);
  connect(
      this, &PlayerImpl::sig_setPort, this, &PlayerImpl::setPort,
      Qt::QueuedConnection);
}

void PlayerImpl::registerPluginPath(std::string s)
{
  m_pluginPath = s;
}

void PlayerImpl::closeDocument()
{
  // Clear existing document
  if (m_currentDocument)
  {
    stop();

    m_execPlugin->clear();

    for (auto dev : m_ownedDevices)
      releaseDevice(dev);

    m_execPlugin = nullptr;
    m_localTreePlugin = nullptr;
    m_devicesPlugin = nullptr;
#if defined(SCORE_ADDON_NETWORK)
    m_networkPlugin = nullptr;
#endif
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
  m_currentDocument = std::make_unique<Document>(
      "Untitled", json, fac, QCoreApplication::instance());

  setupLoadedDocument();
}

void PlayerImpl::loadArray(QByteArray network)
{
  closeDocument();

  Scenario::ScenarioDocumentFactory fac;
  m_currentDocument = std::make_unique<Document>(
      "Untitled", QJsonDocument::fromBinaryData(network).object(), fac,
      QCoreApplication::instance());

  setupLoadedDocument();
}

void PlayerImpl::setupLoadedDocument()
{
  m_documents.documents().push_back(m_currentDocument.get());
  m_documents.setCurrentDocument(m_currentDocument.get());

  // Create execution plug-ins
  const score::DocumentContext& ctx = m_currentDocument->context();
  m_localTreePlugin
      = new LocalTree::DocumentPlugin{ctx, Id<DocumentPlugin>{999}, nullptr};
  m_localTreePlugin->init();
  m_execPlugin
      = new Execution::DocumentPlugin{ctx, Id<DocumentPlugin>{998}, nullptr};

  DocumentModel& doc_model = m_currentDocument->model();
  doc_model.addPluginModel(m_localTreePlugin);
  doc_model.addPluginModel(m_execPlugin);

#if defined(SCORE_PLUGIN_AUDIO)
  auto& audio_ctx
      = m_components
            .applicationPlugin<Audio::AudioStreamEngine::ApplicationPlugin>()
            .context();
  doc_model.addPluginModel(new Audio::AudioStreamEngine::DocumentPlugin{
      audio_ctx, ctx, Id<DocumentPlugin>{997}, nullptr});
#endif

  m_devicesPlugin = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();

  SCORE_ASSERT(m_devicesPlugin);
  for (ossia::net::device_base* dev : m_ownedDevices)
  {
    Device::DeviceInterface* d = m_devicesPlugin->list().findDevice(
        QString::fromStdString(dev->get_name()));

    if (auto sd = dynamic_cast<Device::OwningDeviceInterface*>(d))
    {
      sd->replaceDevice(dev);
    }
    else
    {
      m_devicesPlugin->list().apply([](const Device::DeviceInterface& d) {
        qDebug() << d.settings().name;
      });
      ossia::logger().error(
          "Tried to register unknown device: {}", dev->get_name());
    }
  }
}

void PlayerImpl::registerDevice(ossia::net::device_base* dev)
{
  m_ownedDevices.push_back(dev);
}

void PlayerImpl::setPort(int p)
{
#if defined(SCORE_ADDON_NETWORK)
  auto& ns = m_appContext.settings<Network::Settings::Model>();
  ns.setPlayerPort(p);
#endif
}

void PlayerImpl::releaseDevice(ossia::net::device_base* dev)
{
  SCORE_ASSERT(m_devicesPlugin);
  Device::DeviceInterface* d = m_devicesPlugin->list().findDevice(
      QString::fromStdString(dev->get_name()));
  if (auto sd = dynamic_cast<Device::OwningDeviceInterface*>(d))
  {
    sd->releaseDevice();
  }
}

void PlayerImpl::exec()
{
  if (m_app)
    m_app->exec();
}

void PlayerImpl::close()
{
  if (m_app)
    m_app->exit(0);
}

void PlayerImpl::prepare_play()
{
  DocumentModel& doc_model = m_currentDocument->model();
  Scenario::IntervalModel& root_cst
      = safe_cast<Scenario::ScenarioDocumentModel&>(doc_model.modelDelegate())
            .baseInterval();
  m_execPlugin->reload(root_cst);
  auto& exec_ctx = m_execPlugin->context();

  auto& exec_settings = m_appContext.settings<Execution::Settings::Model>();
  m_clock = exec_settings.makeClock(exec_ctx);
}

void PlayerImpl::do_play()
{
  m_clock->play(TimeVal::zero());
}

void PlayerImpl::stop()
{
  if (m_clock)
    m_clock->stop();

#if defined(SCORE_ADDON_NETWORK)
  if (m_networkPlugin)
    m_networkPlugin->on_stop();
#endif

  if (m_execPlugin)
    m_execPlugin->clear();

  m_clock.reset();
}

const ApplicationContext& PlayerImpl::context() const
{
  return m_appContext;
}

const ApplicationComponents& PlayerImpl::components() const
{
  return m_appContext.components;
}

void PlayerImpl::loadPlugins(
    ApplicationRegistrar& registrar, const ApplicationContext& context)
{
  using namespace score;
  using namespace PluginLoader;
  // Here, the plug-ins that are effectively loaded.
  std::vector<Addon> availablePlugins;

  // Load static plug-ins
  for (QObject* plugin : QPluginLoader::staticInstances())
  {
    if (auto score_plug = dynamic_cast<Plugin_QtInterface*>(plugin))
    {
      Addon addon;
      addon.corePlugin = true;
      addon.plugin = score_plug;
      addon.key = score_plug->key();
      addon.corePlugin = true;
      availablePlugins.push_back(std::move(addon));
    }
  }

  if (!m_pluginPath.empty())
    loadPluginsInAllFolders(
        availablePlugins, {QString::fromStdString(m_pluginPath)});
  else
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
  if (!add.empty())
  {

    for (const Addon& addon : availablePlugins)
    {
      auto ctrl_plugin
          = dynamic_cast<ApplicationPlugin_QtInterface*>(addon.plugin);
      if (ctrl_plugin)
      {
        if (auto plug = ctrl_plugin->make_applicationPlugin(context))
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

Player::Player() : Player{std::string{}}
{
}

Player::Player(std::string plugin_path)
{
  if (QCoreApplication::instance())
  { // we run in the main thread
    m_player = std::make_unique<PlayerImpl>();

    m_player->registerPluginPath(plugin_path);

    m_player->init();
    m_loaded = true;
  }
  else
  {
    m_thread = std::thread([&, plugins = plugin_path] {
      m_player = std::make_unique<PlayerImpl>(true);

      m_player->registerPluginPath(plugins);

      m_player->init();
      m_loaded = true;
      m_player->exec();
      m_player.reset();
    });
  }
}

Player::~Player()
{
  if (m_player)
    m_player->sig_close();

  if (m_thread.joinable())
    m_thread.join();

  m_player.reset();
}

void Player::setPort(int port)
{
  assert(m_loaded);
  m_player->sig_setPort(port);
}

void Player::load(std::string path)
{
  while (!m_loaded)
    ;
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
