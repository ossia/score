// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "player.hpp"

#include "player_impl.hpp"

#include <Device/Protocol/DeviceInterface.hpp>

#include <Scenario/Process/ScenarioModel.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <Execution/Clock/DataflowClock.hpp>
#include <Execution/ExecutionController.hpp>
#include <Execution/Settings/ExecutorModel.hpp>

#include <score/model/ComponentSerialization.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>

#include <ossia/detail/logger.hpp>
#include <ossia/network/base/device.hpp>

#include <QFile>
#if defined(SCORE_PLUGIN_AUDIO)
#include <Audio/Settings/Model.hpp>
#endif

#if defined(SCORE_ADDON_NETWORK)

#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>

#include <Network/Document/ClientPolicy.hpp>
#include <Network/Document/DocumentPlugin.hpp>
#include <Network/Document/Execution/BasicPruner.hpp>
#include <Network/PlayerPlugin.hpp>
#include <Network/Settings/NetworkSettingsModel.hpp>
#endif
#include <wobjectimpl.h>

#if defined(SCORE_STATIC_PLUGINS)
#include <score_static_plugins.hpp>
#endif
W_OBJECT_IMPL(score::PlayerImpl)

class QConfFileSettingsPrivate
{
  void flush();
};
void QConfFileSettingsPrivate::flush() { }

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

#if defined(SCORE_STATIC_PLUGINS)
  score_init_static_plugins();
#endif
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
  srand(time(NULL));

  m_globSettings.tryToRestore = false;
  m_globSettings.gui = false;
  m_globSettings.opengl = false;
  m_globSettings.autoplay = false;

  ossia::context c;
  // Load global plug-in data
  ApplicationRegistrar reg(m_compData);
  reg.registerFactory(std::make_unique<DocumentDelegateList>());
  reg.registerFactory(std::make_unique<ValidityCheckerList>());
#if defined(SCORE_SERIALIZABLE_COMPONENTS)
  reg.registerFactory(std::make_unique<SerializableComponentFactoryList>());
#endif
  reg.registerFactory(std::make_unique<DocumentPluginFactoryList>());
  reg.registerFactory(std::make_unique<SettingsDelegateFactoryList>());

  loadPlugins(reg, m_appContext);

  QSettings s("ossia.io", "player");
  // s.setValue(
  //     "Audio/Driver", QVariant::fromValue(Audio::AudioFactory::ConcreteKey{
  //                         score::uuids::string_generator::compute(
  //                             "28b88e91-c5f0-4f13-834f-aa333d14aa81")}));
  s.setValue(
      "score_plugin_engine/Clock",
      QVariant::fromValue(Dataflow::ClockFactory::static_concreteKey()));
  s.setValue(
      "Network/ClientName",
      QString::fromStdString(fmt::format("player.{}", rand() % 10000)));
  for(auto& plugin : m_appContext.interfaces<SettingsDelegateFactoryList>())
  {
    m_settings.push_back(plugin.makeModel(s, m_appContext));
  }

  m_appContext.forAppPlugins([](auto& app_plug) { app_plug.initialize(); });

#if defined(SCORE_ADDON_NETWORK)
  auto& netplug = m_components.applicationPlugin<Network::PlayerPlugin>();
  netplug.documentLoader = [&](const QByteArray& arr) {
    loadArray(arr);
    return m_currentDocument.get();
  };
  netplug.onDocumentLoaded = [&] {
    Document& doc = *m_currentDocument;
    auto plug = doc.context().findPlugin<Network::NetworkDocumentPlugin>();
    if(plug)
    {
      m_networkPlugin = plug;
      auto pol = dynamic_cast<Network::PlayerClientEditionPolicy*>(&plug->policy());
      if(pol)
      {
        pol->onPlay = [this] {
          prepare_play();

          auto& exec_ctx = m_execPlugin->context();
          m_execPlugin->runAllCommands();
          Network::BasicPruner{*m_networkPlugin}(
              exec_ctx, *m_execPlugin->baseScenario());
          m_execPlugin->runAllCommands();

          do_play();
        };
        pol->onStop = [this] { stop(); };
      }
    }
  };

#endif
  connect(this, &PlayerImpl::sig_play, this, &PlayerImpl::play, Qt::QueuedConnection);
  connect(this, &PlayerImpl::sig_stop, this, &PlayerImpl::stop, Qt::QueuedConnection);
  connect(
      this, &PlayerImpl::sig_loadFile, this, &PlayerImpl::loadFile,
      Qt::QueuedConnection);
  connect(this, &PlayerImpl::sig_close, this, &PlayerImpl::close, Qt::QueuedConnection);
  connect(
      this, &PlayerImpl::sig_registerDevice, this, &PlayerImpl::registerDevice,
      Qt::QueuedConnection);
  connect(
      this, &PlayerImpl::sig_setPort, this, &PlayerImpl::setPort, Qt::QueuedConnection);
}

void PlayerImpl::registerPluginPath(std::string s)
{
  m_pluginPath = s;
}

void PlayerImpl::closeDocument()
{
  // Clear existing document
  if(m_currentDocument)
  {
    stop();

    //m_execPlugin->clear();

    for(auto dev : m_ownedDevices)
      releaseDevice(dev);

    m_execPlugin = nullptr;
    m_localTreePlugin = nullptr;
    m_devicesPlugin = nullptr;
#if defined(SCORE_ADDON_NETWORK)
    m_networkPlugin = nullptr;
#endif
    m_documents.documents().clear();
    static_cast<DocumentList&>(m_documents).setCurrentDocument(nullptr);
    m_currentDocument.reset();
  }
}

void PlayerImpl::loadFile(QString file)
{
  closeDocument();

  // Load new document
  QFile f(file);
  f.open(QIODevice::ReadOnly);

  Scenario::ScenarioDocumentFactory fac;
  m_currentDocument.reset(m_documents.loadPlayerDocument(
      m_appContext, "Untitled", f.readAll(), JSONObject::type(), fac));

  setupLoadedDocument();
}

void PlayerImpl::loadArray(QByteArray network)
{
  closeDocument();

  Scenario::ScenarioDocumentFactory fac;
  m_currentDocument.reset(m_documents.loadPlayerDocument(
      m_appContext, "Untitled", network, JSONObject::type(), fac));

  setupLoadedDocument();
}

void PlayerImpl::setupLoadedDocument()
{
  assert(m_currentDocument);
  m_documents.documents().push_back(m_currentDocument.get());
  static_cast<score::DocumentList&>(m_documents)
      .setCurrentDocument(m_currentDocument.get());

  // Create execution plug-ins
  const score::DocumentContext& ctx = m_currentDocument->context();
  assert(((std::intptr_t)&ctx.document) != 0);

  m_devicesPlugin = &ctx.plugin<Explorer::DeviceDocumentPlugin>();
  m_localTreePlugin = &ctx.plugin<LocalTree::DocumentPlugin>();
  m_execPlugin = &ctx.plugin<Execution::DocumentPlugin>();
  // m_localTreePlugin = new LocalTree::DocumentPlugin{ctx, nullptr};
  // m_localTreePlugin->init();

  qDebug() << "Ready to play";

  for(ossia::net::device_base* dev : m_ownedDevices)
  {
    Device::DeviceInterface* d
        = m_devicesPlugin->list().findDevice(QString::fromStdString(dev->get_name()));

    if(auto sd = dynamic_cast<Device::OwningDeviceInterface*>(d))
    {
      sd->replaceDevice(dev);
    }
    else
    {
      m_devicesPlugin->list().apply(
          [](const Device::DeviceInterface& d) { qDebug() << d.settings().name; });
      ossia::logger().error("Tried to register unknown device: {}", dev->get_name());
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
  Device::DeviceInterface* d
      = m_devicesPlugin->list().findDevice(QString::fromStdString(dev->get_name()));
  if(auto sd = dynamic_cast<Device::OwningDeviceInterface*>(d))
  {
    sd->releaseDevice();
  }
}

void PlayerImpl::exec()
{
  if(m_app)
    m_app->exec();
}

void PlayerImpl::close()
{
  if(m_app)
    m_app->exit(0);
}

void PlayerImpl::prepare_play() { }

void PlayerImpl::do_play()
{
  this->context()
      .applicationPlugin<Engine::ApplicationPlugin>()
      .execution()
      .request_play_global(true);
}

void PlayerImpl::stop()
{
  this->context()
      .applicationPlugin<Engine::ApplicationPlugin>()
      .execution()
      .request_stop();
#if defined(SCORE_ADDON_NETWORK)
  if(m_networkPlugin)
    m_networkPlugin->on_stop();
#endif
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

#if defined(SCORE_STATIC_PLUGINS)

  // Load static plug-ins
  for(auto score_plug : score::staticPlugins())
  {
    score::Addon addon;
    addon.corePlugin = true;
    addon.plugin = score_plug;
    addon.key = score_plug->key();
    availablePlugins.push_back(std::move(addon));
  }
#endif

  if(!m_pluginPath.empty())
    loadPluginsInAllFolders(availablePlugins, {QString::fromStdString(m_pluginPath)});
  else
    loadPluginsInAllFolders(availablePlugins);

  loadAddonsInAllFolders(availablePlugins);

  registrar.registerAddons(availablePlugins);

  for(const Addon& addon : availablePlugins)
  {
    auto facfam_interface = dynamic_cast<FactoryList_QtInterface*>(addon.plugin);

    if(facfam_interface)
    {
      for(auto&& elt : facfam_interface->factoryFamilies())
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
      auto ctrl_plugin = dynamic_cast<ApplicationPlugin_QtInterface*>(addon.plugin);
      if(ctrl_plugin)
      {
        if(auto plug = ctrl_plugin->make_applicationPlugin(context))
          registrar.registerApplicationPlugin(plug);
      }
    }

    for(const Addon& addon : availablePlugins)
    {
      if(auto commands_plugin
         = dynamic_cast<score::CommandFactory_QtInterface*>(addon.plugin))
      {
        auto [key, cmds] = commands_plugin->make_commands();
        registrar.registerCommands(key, std::move(cmds));
      }

      auto factories_plugin = dynamic_cast<FactoryInterface_QtInterface*>(addon.plugin);
      if(factories_plugin)
      {
        for(auto& factory_family : registrar.components().factories)
        {
          for(auto&& new_factory :
              factories_plugin->factories(context, factory_family.first))
          {
            factory_family.second->insert(std::move(new_factory));
          }
        }
      }
    }
  }
}

Player::Player(std::function<void()> onReady)
    : Player{std::string{}, onReady}
{
}

Player::Player(std::string plugin_path, std::function<void()> onReady)
{
  if(QCoreApplication::instance())
  { // we run in the main thread
    m_player = std::make_unique<PlayerImpl>();

    m_player->registerPluginPath(plugin_path);

    m_player->init();
    m_loaded = true;
    onReady();
  }
  else
  {
    m_thread = std::thread([&, plugins = plugin_path, onReady] {
      m_player = std::make_unique<PlayerImpl>(true);

      m_player->registerPluginPath(plugins);

      m_player->init();
      m_loaded = true;
      onReady();
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

void Player::setPort(int port)
{
  assert(m_loaded);
  assert(m_player);
  m_player->sig_setPort(port);
}

void Player::load(std::string path)
{
  assert(m_player);
  while(!m_loaded)
    ;
  m_player->sig_loadFile(QString::fromStdString(path));
}

void Player::play()
{
  assert(m_loaded);
  assert(m_player);
  m_player->sig_play();
}

void Player::stop()
{
  assert(m_loaded);
  assert(m_player);
  m_player->sig_stop();
}

void Player::registerDevice(ossia::net::device_base& dev)
{
  assert(m_loaded);
  assert(m_player);
  m_player->sig_registerDevice(&dev);
}
}
