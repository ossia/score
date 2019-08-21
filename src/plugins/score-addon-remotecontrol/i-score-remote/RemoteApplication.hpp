#pragma once
#include <score/serialization/StringConstants.hpp>

#include <QAbstractListModel>
#include <QQmlApplicationEngine>
#include <QtWebSockets/QtWebSockets>

#include <wobjectdefs.h>

#include <functional>
class QApplication;

struct SyncInfo
{
  QJsonValue path;
  QString prettyName;
  friend bool operator==(const SyncInfo& tn, const QJsonValue& rhs)
  {
    return tn.path == rhs;
  }
  friend bool operator!=(const SyncInfo& tn, const QJsonValue& rhs)
  {
    return tn.path != rhs;
  }
};

class TriggerList : public QAbstractListModel
{
public:
  template <typename Fun>
  void apply(Fun f)
  {
    beginResetModel();
    f();
    endResetModel();
  }

  int rowCount(const QModelIndex&) const override { return timeSyncs.size(); }

  QVariant data(const QModelIndex& index, int) const override
  {
    if (index.row() >= timeSyncs.size())
      return {};

    return timeSyncs[index.row()].prettyName;
  }

  QHash<int, QByteArray> roleNames() const override
  {
    QHash<int, QByteArray> hash;
    hash.insert(Qt::DisplayRole, "name");
    return hash;
  }
  std::vector<SyncInfo> timeSyncs;
};

struct WebSocketHandler : public QObject
{
  W_OBJECT(WebSocketHandler)

public:
  TriggerList m_activeSyncs;

private:
  QWebSocket m_server;
  std::map<QString, std::function<void(const QJsonObject&)>> m_answers;

public:
  WebSocketHandler()
  {
    m_answers.insert(
        std::make_pair("TriggerAdded", [this](const QJsonObject& obj) {
          qDebug() << obj;
          auto json_it = obj.find("Path");
          if (json_it == obj.end())
            return;

          auto it = std::find(
              m_activeSyncs.timeSyncs.begin(),
              m_activeSyncs.timeSyncs.end(),
              *json_it);
          if (it == m_activeSyncs.timeSyncs.end())
          {
            m_activeSyncs.apply([=]() {
              m_activeSyncs.timeSyncs.emplace_back(SyncInfo{
                  *json_it, obj[score::StringConstant().Name].toString()});
            });
          }
        }));

    m_answers.insert(
        std::make_pair("TriggerRemoved", [this](const QJsonObject& obj) {
          qDebug() << obj;
          auto json_it = obj.find("Path");
          if (json_it == obj.end())
            return;

          auto it = std::find(
              m_activeSyncs.timeSyncs.begin(),
              m_activeSyncs.timeSyncs.end(),
              *json_it);
          if (it != m_activeSyncs.timeSyncs.end())
          {
            m_activeSyncs.apply([=]() { m_activeSyncs.timeSyncs.erase(it); });
          }
        }));

    connect(
        &m_server,
        &QWebSocket::textMessageReceived,
        this,
        &WebSocketHandler::processTextMessage);
    connect(
        &m_server,
        &QWebSocket::binaryMessageReceived,
        this,
        &WebSocketHandler::processBinaryMessage);
    connect(&m_server, &QWebSocket::connected, this, [] { qDebug("yolooo"); });
    connect(
        &m_server,
        static_cast<void (QWebSocket::*)(QAbstractSocket::SocketError)>(
            &QWebSocket::error),
        this,
        [=](QAbstractSocket::SocketError) {
          qDebug() << m_server.errorString();
        });

    m_server.open(QUrl("ws://147.210.128.72:10212"));
  }

  ~WebSocketHandler() { m_server.close(); }

  void processTextMessage(const QString& message)
  {
    processBinaryMessage(message.toLatin1());
  }

  void processBinaryMessage(QByteArray message)
  {
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(std::move(message), &error);
    if (error.error)
      return;

    auto obj = doc.object();
    auto it = obj.find("Message");
    if (it == obj.end())
      return;

    auto mess = it->toString();
    auto answer_it = m_answers.find(mess);
    if (answer_it == m_answers.end())
      return;

    answer_it->second(obj);
  }

  void on_rowPressed(int i)
  {
    if (i >= m_activeSyncs.timeSyncs.size())
      return;

    auto tn = m_activeSyncs.timeSyncs[i];

    QJsonObject mess;
    mess[score::StringConstant().Message] = "Trigger";
    mess[score::StringConstant().Path] = tn.path;
    QJsonDocument doc{mess};
    auto json = doc.toJson();

    m_server.sendTextMessage(json);
  }
  W_SLOT(on_rowPressed)

  void on_play()
  {
    QJsonObject mess;
    mess[score::StringConstant().Message] = "Play";
    m_server.sendTextMessage(QJsonDocument{mess}.toJson());
  }
  W_SLOT(on_play)

  void on_pause()
  {
    QJsonObject mess;
    mess[score::StringConstant().Message] = "Pause";
    m_server.sendTextMessage(QJsonDocument{mess}.toJson());
  }
  W_SLOT(on_pause)

  void on_stop()
  {
    QJsonObject mess;
    mess[score::StringConstant().Message] = "Stop";
    m_server.sendTextMessage(QJsonDocument{mess}.toJson());
  }
  W_SLOT(on_stop)

  void on_addressChanged(QString addr)
  {
    m_server.close();

    m_activeSyncs.apply([this]() { m_activeSyncs.timeSyncs.clear(); });

    m_server.open(QUrl{addr});
  }
  W_SLOT(on_addressChanged)
};

class RemoteApplication final : QObject
{
public:
  RemoteApplication(int& argc, char** argv);
  ~RemoteApplication();

  int exec();

  // Base stuff.
  QApplication* m_app;

  QQmlApplicationEngine engine;
  WebSocketHandler m_triggers;
};
