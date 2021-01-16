#pragma once
#include <State/Message.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/std/StringHash.hpp>

#include <ossia/detail/flat_map.hpp>
#include <nano_observer.hpp>

#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>

#include <score_plugin_remotecontrol_export.h>
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
  friend bool operator==(const WSClient& lhs, const WSClient& rhs) noexcept
  {
    return lhs.socket == rhs.socket;
  }
};

struct Handler
{
  ossia::flat_map<QString, std::function<void(const rapidjson::Value&, const WSClient&)>> answers;

  std::function<void(const std::vector<WSClient>&)> onAdded;
  std::function<void(const std::vector<WSClient>&)> onRemoved;
  std::function<void(const WSClient&)> onClientConnection;
  std::function<void(const WSClient&)> onClientDisconnection;

  /**
   * @brief Helper function to set handlers from a pair of init / deinit functions
   */
  template<typename T>
  void setupDefaultHandler(T msgs)
  {
    onAdded = [msgs] (const std::vector<RemoteControl::WSClient>& clts) {
      auto msg = msgs.initMessage();
      for(auto& clt : clts)
        clt.socket->sendTextMessage(msg);
    };
    onRemoved = [msgs] (const std::vector<RemoteControl::WSClient>& clts) {
      auto msg = msgs.deinitMessage();
      for(auto& clt : clts)
        clt.socket->sendTextMessage(msg);
    };

    onClientConnection = [msgs] (const RemoteControl::WSClient& clt) {
      auto msg = msgs.initMessage();
      clt.socket->sendTextMessage(msg);
    };
    onClientDisconnection = [msgs] (const RemoteControl::WSClient& clt) {
      auto msg = msgs.deinitMessage();
      clt.socket->sendTextMessage(msg);
    };
  }
};

struct SCORE_PLUGIN_REMOTECONTROL_EXPORT Receiver : public QObject, public Nano::Observer
{
public:
  explicit Receiver(const score::DocumentContext& doc, quint16 port);

  ~Receiver();

  void addHandler(QObject* context, Handler&& handler);
  void removeHandler(QObject* context);

  void registerSync(Path<Scenario::TimeSyncModel> tn);
  void unregisterSync(Path<Scenario::TimeSyncModel> tn);

  void onNewConnection();

  void processTextMessage(const QString& message, const WSClient& w);
  void processBinaryMessage(QByteArray message, const WSClient& w);

  void socketDisconnected();

private:
  void on_valueUpdated(const ::State::Address& addr, const ossia::value& v);

  QWebSocketServer m_server;
  std::vector<WSClient> m_clients;

  Explorer::DeviceDocumentPlugin& m_dev;
  std::list<Path<Scenario::TimeSyncModel>> m_activeSyncs;

  score::hash_map<QString, std::function<void(const rapidjson::Value&, const WSClient&)>> m_answers;
  score::hash_map<::State::Address, WSClient> m_listenedAddresses;

  std::vector<std::pair<QObject*, Handler>> m_handlers;
};

class DocumentPlugin : public score::DocumentPlugin
{
public:
  DocumentPlugin(const score::DocumentContext& doc, Id<score::DocumentPlugin> id, QObject* parent);
  ~DocumentPlugin();

  void on_documentClosing() override;
  Receiver receiver;

private:
  void create();
  void cleanup();

  Interval* m_root{};
};
}
