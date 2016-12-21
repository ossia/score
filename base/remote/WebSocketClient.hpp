#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <QtWebSockets>
#include <unordered_map>
#include <string>
#include <functional>
namespace RemoteUI
{
using json_fun = std::function<void(const QJsonObject&)>;
class WebSocketClient : public QObject
{
    Q_OBJECT
public:
  WebSocketClient();
  ~WebSocketClient();
  void open(QUrl u);
  QWebSocket& socket() { return m_ws; }

  std::unordered_map<std::string, json_fun> actions;

private:
  void handleObject(const QJsonObject& obj);

  QWebSocket m_ws;
};
}
#endif // WEBSOCKETCLIENT_H
