#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <QGraphicsScene>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>

class WebSocketView final : public QObject
{
  Q_OBJECT
public:
  explicit WebSocketView(
      QGraphicsScene* s, quint16 port, QObject* parent = Q_NULLPTR);
  ~WebSocketView();

Q_SIGNALS:
  void closed();

public Q_SLOTS:
  void onNewConnection();
  void processTextMessage(QString message);
  void processBinaryMessage(QByteArray message);
  void socketDisconnected();

private:
  QWebSocketServer* m_pWebSocketServer;
  QGraphicsScene* m_scene{};
  QList<QWebSocket*> m_clients;
  bool m_debug;
};
