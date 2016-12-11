#include "RemoteApplication.hpp"
#include <QJsonDocument>
#include <QVector>
#include <QQmlContext>
#include <QDebug>
#include <QQmlComponent>
#include <QQmlProperty>
#include <core/application/ApplicationRegistrar.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <core/plugin/PluginManager.hpp>
namespace RemoteUI
{

RemoteApplication::RemoteApplication(int& argc, char** argv):
  m_app{argc, argv},
  m_engine{},
  m_appContext{m_applicationSettings, m_components, m_settings},
  m_context{m_engine, m_widgets.componentList, m_nodes, m_ws, [&] {

    m_engine.rootContext()->setContextProperty(
               "factoriesModel", QVariant::fromValue(m_widgets.objectList));

    m_engine.rootContext()->setContextProperty(
               "nodesModel", QVariant::fromValue((QAbstractItemModel*)&m_nodes));

    m_engine.load(QUrl(QLatin1String("qrc:///qml/main.qml")));

    auto& root = *m_engine.rootObjects()[0];
    return root.findChild<QQuickItem*>("centralItem", Qt::FindChildrenRecursively);
  }()},
  m_centralItemModel{m_context, nullptr}
{
  m_instance = this;

  using namespace std::literals;
  m_ws.actions.insert(
        std::make_pair(
          "DeviceTree"s,
          json_fun{[this] (const QJsonObject& json) {
    Deserializer<JSONObject> wr{json["Nodes"].toObject()};
    Device::Node n;
    wr.writeTo(n);
    m_nodes.replace(n);
  }}));

  loadPlugins();

  m_ws.open(QUrl("ws://127.0.0.1:10212"));
}

RemoteApplication::~RemoteApplication()
{

}

const iscore::ApplicationContext& RemoteApplication::context() const
{
  throw;
}

const iscore::ApplicationComponents& RemoteApplication::components() const
{ return m_components; }

int RemoteApplication::exec()
{
  return m_app.exec();
}

struct RemoteRegistrar
{

  RemoteRegistrar(
      iscore::ApplicationComponentsData& c): m_components{c} { }

  // Register data from plugins
  void registerAddons(std::vector<iscore::Addon> vec) { }
  void registerApplicationContextPlugin(iscore::GUIApplicationContextPlugin*) { }
  void registerPanel(iscore::PanelDelegateFactory&) { }
  void registerCommands(
      iscore::hash_map<CommandGroupKey, CommandGeneratorMap>&& cmds) { }
  void registerCommands(
      std::pair<CommandGroupKey, CommandGeneratorMap>&& cmds) { }
  void registerFactories(
      iscore::hash_map<iscore::InterfaceKey, std::unique_ptr<iscore::InterfaceListBase>>&&
              facts)
  {
    m_components.factories = std::move(facts);
  }

  void registerFactory(std::unique_ptr<iscore::InterfaceListBase> cmds)
  {
    m_components.factories.insert(
        std::make_pair(cmds->interfaceKey(), std::move(cmds)));
  }

  auto& components() const { return m_components; }


private:
  iscore::ApplicationComponentsData& m_components;
};

void RemoteApplication::loadPlugins()
{
  RemoteRegistrar registrar{m_compData};
  registrar.registerFactory(
      std::make_unique<iscore::DocumentPluginFactoryList>());
  registrar.registerFactory(
      std::make_unique<iscore::SettingsDelegateFactoryList>());

}

}
