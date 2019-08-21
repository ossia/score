#pragma once
#include <State/Message.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>

#include <nano_observer.hpp>
template <typename T>
class TreeNode;
namespace Device
{
class DeviceExplorerNode;
using Node = TreeNode<DeviceExplorerNode>;
}
namespace Explorer
{
class DeviceDocumentPlugin;
}
namespace Scenario
{
class TimeSyncModel;
}
namespace RemoteControl
{
class Interval;

struct WSClient
{
  QWebSocket* socket{};
  friend bool operator==(const WSClient& lhs, const WSClient& rhs)
  {
    return lhs.socket == rhs.socket;
  }
};

struct Receiver : public QObject, public Nano::Observer
{
public:
  explicit Receiver(const score::DocumentContext& doc, quint16 port);

  ~Receiver();

  void registerSync(Path<Scenario::TimeSyncModel> tn);

  void unregisterSync(Path<Scenario::TimeSyncModel> tn);

  void onNewConnection();

  void processTextMessage(const QString& message, const WSClient& w);

  void processBinaryMessage(QByteArray message, const WSClient& w);

  void socketDisconnected();

private:
  void on_valueUpdated(const ::State::Address& addr, const ossia::value& v);

  QWebSocketServer m_server;
  QList<WSClient> m_clients;

  Explorer::DeviceDocumentPlugin& m_dev;
  std::list<Path<Scenario::TimeSyncModel>> m_activeSyncs;

  std::map<QString, std::function<void(const QJsonObject&, const WSClient&)>>
      m_answers;
  score::hash_map<::State::Address, WSClient> m_listenedAddresses;
};

class DocumentPlugin : public score::DocumentPlugin
{
public:
  DocumentPlugin(
      const score::DocumentContext& doc,
      Id<score::DocumentPlugin> id,
      QObject* parent);

  ~DocumentPlugin();

  void on_documentClosing() override;
  Receiver receiver;

private:
  void create();
  void cleanup();

  Interval* m_root{};
};
}
