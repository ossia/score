#include "WebSocketClient.hpp"
#include <iscore/serialization/StringConstants.hpp>

namespace RemoteUI
{

WebSocketClient::WebSocketClient():
  m_ws{"remote-ui"}
{
  // Websocket setup
  connect(&m_ws, &QWebSocket::binaryMessageReceived,
          this, [=] (const QByteArray& arr) {
    auto doc = QJsonDocument::fromBinaryData(arr);
    if(doc.isObject())
    {
      handleObject(doc.object());
    }
  });
  connect(&m_ws, &QWebSocket::textMessageReceived,
          this, [=] (const QString& mess) {

    auto doc = QJsonDocument::fromJson(mess.toUtf8());
    if(doc.isObject())
    {
      handleObject(doc.object());
    }
  });

  connect(&m_ws, &QWebSocket::connected,
          this, [] { qDebug("Connected"); });
  connect(&m_ws, static_cast<void(QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error),
          this , [=](QAbstractSocket::SocketError) { qDebug() << m_ws.errorString(); });
  connect(&m_ws, &QWebSocket::disconnected,
          this, [] {

    qDebug() << "disconnected";
  });

}

WebSocketClient::~WebSocketClient()
{

}

void WebSocketClient::open(QUrl u)
{
  m_ws.open(u);
}

void WebSocketClient::handleObject(const QJsonObject& obj)
{
  qDebug() << obj;
  auto it = actions.find(obj[iscore::StringConstant().Message].toString().toStdString());
  if(it != actions.end())
  {
    ((*it).second)(obj);
  }
}


}
