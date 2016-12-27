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
#include <Device/Address/AddressSettings.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <Models/GUIItem.hpp>
namespace RemoteUI
{

RemoteApplication::RemoteApplication(int& argc, char** argv):
  m_app{argc, argv},
  m_engine{},
  m_appContext{m_applicationSettings, m_components, m_settings},
  m_context{m_engine, m_widgets.componentList, m_nodes, m_ws, *this, [&] {

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
    JSONObject::Deserializer wr{json["Nodes"].toObject()};
    Device::Node n;
    wr.writeTo(n);
    m_nodes.replace(n);
  }}));

  m_ws.actions.insert(
        std::make_pair(
          "Message"s,
          json_fun{[this] (const QJsonObject& json) {
  auto m = unmarshall<State::Message>(json);
                     qDebug() << m;
  auto it = m_listening.find(m.address.address);
  if(it != m_listening.end())
    it->second->setValue(m);
  }}));

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

void RemoteApplication::enableListening(const Device::FullAddressSettings& a, GUIItem* i)
{
  if(!a.address.path.empty())
  {
    QJsonObject obj;
    obj[iscore::StringConstant().Message] = "EnableListening";
    obj[iscore::StringConstant().Address] = marshall<JSONObject>(a.address);
    m_ws.socket().sendTextMessage(QJsonDocument(obj).toJson());

    m_listening[a.address] = i;
  }
}

void RemoteApplication::disableListening(const Device::FullAddressSettings& a, GUIItem* i)
{
  if(!a.address.path.empty())
  {
    QJsonObject obj;
    obj[iscore::StringConstant().Message] = "DisableListening";
    obj[iscore::StringConstant().Address] = marshall<JSONObject>(a.address);
    m_ws.socket().sendTextMessage(QJsonDocument(obj).toJson());
  }

  auto it = m_listening.find(a.address);
  if(it != m_listening.end())
    m_listening.erase(it);
}
}
