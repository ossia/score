#pragma once
#include <State/Message.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/std/StringHash.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>

#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>

#include <nano_observer.hpp>
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
class IntervalModel;
class TimeSyncModel;
}
namespace RemoteControl::Settings
{
class Model;
}
namespace RemoteControl::WS
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
  ossia::flat_map<QString, std::function<void(const rapidjson::Value&, const WSClient&)>>
      answers;

  std::function<void(const std::vector<WSClient>&)> onAdded;
  std::function<void(const std::vector<WSClient>&)> onRemoved;
  std::function<void(const WSClient&)> onClientConnection;
  std::function<void(const WSClient&)> onClientDisconnection;

  /**
   * @brief Helper function to set handlers from a pair of init / deinit functions
   */
  template <typename T>
  void setupDefaultHandler(T msgs)
  {
    onAdded = [msgs](const std::vector<RemoteControl::WS::WSClient>& clts) {
      auto msg = msgs.initMessage();
      for(auto& clt : clts)
        clt.socket->sendTextMessage(msg);
    };
    onRemoved = [msgs](const std::vector<RemoteControl::WS::WSClient>& clts) {
      auto msg = msgs.deinitMessage();
      for(auto& clt : clts)
        clt.socket->sendTextMessage(msg);
    };

    onClientConnection = [msgs](const RemoteControl::WS::WSClient& clt) {
      auto msg = msgs.initMessage();
      clt.socket->sendTextMessage(msg);
    };
    onClientDisconnection = [msgs](const RemoteControl::WS::WSClient& clt) {
      auto msg = msgs.deinitMessage();
      clt.socket->sendTextMessage(msg);
    };
  }
};

//! What the server needs from the settings, resolved when it is opened.
struct ReceiverSettings
{
  int port{10212};

  //! Which interface to serve. score assumes a trusted network, so every
  //! interface by default; a stricter deployment narrows it here.
  QString address{"0.0.0.0"};

  //! Presented by the client as ?token=... on the connection URL.
  QString token;

  //! Whether a client may evaluate JavaScript here. That is control of the
  //! machine score runs on, not of the score.
  bool allowScripting{false};
};

struct SCORE_PLUGIN_REMOTECONTROL_EXPORT Receiver
    : public QObject
    , public Nano::Observer
{
public:
  explicit Receiver(const score::DocumentContext& doc);

  ~Receiver();

  //! Start listening. Closes first if already open, so this doubles as
  //! "apply the settings again".
  void open(const ReceiverSettings& settings);
  void close();
  bool isOpen() const noexcept;

  //! The port actually bound, which differs from the requested one when 0 was
  //! asked for.
  quint16 port() const noexcept;

  void addHandler(QObject* context, Handler&& handler);
  void removeHandler(QObject* context);

  void registerSync(Path<Scenario::TimeSyncModel> tn);
  void unregisterSync(Path<Scenario::TimeSyncModel> tn);

  void onNewConnection();

  void processTextMessage(const QString& message, const WSClient& w);
  void processBinaryMessage(QByteArray message, const WSClient& w);

  void sendMessage(const QString& str);

  void socketDisconnected();

  const std::vector<WSClient>& clients() const noexcept { return m_clients; }

private:
  void on_valueUpdated(const ::State::Address& addr, const ossia::value& v);

  //! Whether this socket presented the right token when it connected.
  bool authorize(const QWebSocket& socket) const noexcept;

  QWebSocketServer m_server;
  ReceiverSettings m_settings;
  std::vector<WSClient> m_clients;

  Explorer::DeviceDocumentPlugin& m_dev;
  std::list<Path<Scenario::TimeSyncModel>> m_activeSyncs;

  score::hash_map<QString, std::function<void(const rapidjson::Value&, const WSClient&)>>
      m_answers;
  score::hash_map<::State::Address, WSClient> m_listenedAddresses;

  std::vector<std::pair<QObject*, Handler>> m_handlers;
};

class SCORE_PLUGIN_REMOTECONTROL_EXPORT DocumentPlugin : public score::DocumentPlugin
{
public:
  DocumentPlugin(const score::DocumentContext& doc, QObject* parent);
  ~DocumentPlugin();

  void timerEvent(QTimerEvent* event) override;

  void registerInterval(Scenario::IntervalModel& m);
  void unregisterInterval(Scenario::IntervalModel& m);

  void on_documentClosing() override;

  Receiver receiver;

private:
  void apply(const Settings::Model& set);
  void create();
  void cleanup();

  struct IntervalData
  {
    Scenario::IntervalModel* model;
    const double* progress;
    Path<Scenario::IntervalModel> p;
  };

  ossia::hash_map<int64_t, IntervalData> m_intervals;

  Interval* m_root{};
};
}
