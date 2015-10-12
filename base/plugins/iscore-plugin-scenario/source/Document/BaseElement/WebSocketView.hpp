#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>

#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>
#include <QGraphicsScene>


class WebSocketView : public QObject
{
        Q_OBJECT
    public:
        explicit WebSocketView(QGraphicsScene* s, quint16 port, QObject *parent = Q_NULLPTR);
        ~WebSocketView();

    signals:
        void closed();

    public slots:
        void onNewConnection();
        void processTextMessage(QString message);
        void processBinaryMessage(QByteArray message);
        void socketDisconnected();

    private:
        QWebSocketServer *m_pWebSocketServer;
        QGraphicsScene* m_scene{};
        QList<QWebSocket *> m_clients;
        bool m_debug;
};
