#pragma once

#include <QGraphicsScene>

#include <verdigris>

class WebSocketView final : public QObject
{
  W_OBJECT(WebSocketView)
public:
  explicit WebSocketView(QGraphicsScene* s, quint16 port, QObject* parent = Q_NULLPTR);
  ~WebSocketView();

public:
  void closed() W_SIGNAL(closed);

public:
  void onNewConnection();
  W_SLOT(onNewConnection);
  void processTextMessage(QString message);
  W_SLOT(processTextMessage);
  void processBinaryMessage(QByteArray message);
  W_SLOT(processBinaryMessage);
  void socketDisconnected();
  W_SLOT(socketDisconnected);

private:
  QWebSocketServer* m_pWebSocketServer;
  QGraphicsScene* m_scene{};
  QList<QWebSocket*> m_clients;
  bool m_debug;
};
