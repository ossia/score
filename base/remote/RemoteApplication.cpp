// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "RemoteApplication.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Models/GUIItem.hpp>
#include <QDebug>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlProperty>
#include <QVector>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/plugin/PluginManager.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/serialization/VisitorCommon.hpp>
namespace RemoteUI
{
RemoteApplication::RemoteApplication(int& argc, char** argv)
    : m_app{argc, argv}
    , m_engine{}
    , m_appContext{m_applicationSettings, m_components, m_docs, m_settings}
    , m_context{m_engine,
                m_widgets.componentList,
                m_nodes,
                m_clients,
                *this,
                [&] {
                  m_engine.rootContext()->setContextProperty(
                      "factoriesModel",
                      QVariant::fromValue(m_widgets.objectList));

                  m_engine.rootContext()->setContextProperty(
                      "nodesModel",
                      QVariant::fromValue((QAbstractItemModel*)&m_nodes));

                  m_engine.load(QUrl(QLatin1String("qrc:///qml/main.qml")));

                  auto& root = *m_engine.rootObjects()[0];
                  return root.findChild<QQuickItem*>(
                      "centralItem", Qt::FindChildrenRecursively);
                }()}
    , m_centralItemModel{m_context, nullptr}
    , m_clients{}
    , m_zeroconf{m_context}
{
  m_instance = this;

  using namespace std::literals;
}

RemoteApplication::~RemoteApplication()
{
}

const score::ApplicationContext& RemoteApplication::context() const
{
  throw;
}

const score::ApplicationComponents& RemoteApplication::components() const
{
  return m_components;
}

int RemoteApplication::exec()
{
  return m_app.exec();
}
}
