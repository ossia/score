// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "RemoteApplication.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Models/GUIItem.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <QQmlContext>
namespace RemoteUI
{
RemoteApplication::RemoteApplication(int& argc, char** argv)
    : m_app{argc, argv}
    , m_engine{}
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
}

RemoteApplication::~RemoteApplication()
{
}

int RemoteApplication::exec()
{
  return m_app.exec();
}
}
