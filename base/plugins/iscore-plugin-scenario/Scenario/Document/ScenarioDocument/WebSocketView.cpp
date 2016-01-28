#include "WebSocketView.hpp"
#include <QtCore/QDebug>

WebSocketView::WebSocketView(QGraphicsScene* s, quint16 port, QObject *parent) :
    QObject(parent),
    m_pWebSocketServer(new QWebSocketServer(QStringLiteral("Echo Server"),
                                            QWebSocketServer::NonSecureMode, this)),
    m_scene{s}
{
    if (m_pWebSocketServer->listen(QHostAddress::Any, port)) {
        if (m_debug)
            qDebug() << "WebSocketView listening on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &WebSocketView::onNewConnection);
        connect(m_pWebSocketServer, &QWebSocketServer::closed, this, &WebSocketView::closed);
    }
}

WebSocketView::~WebSocketView()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

void WebSocketView::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

    connect(pSocket, &QWebSocket::textMessageReceived, this, &WebSocketView::processTextMessage);
    connect(pSocket, &QWebSocket::binaryMessageReceived, this, &WebSocketView::processBinaryMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &WebSocketView::socketDisconnected);

    m_clients << pSocket;
}

#include <QBuffer>
#include <QSvgGenerator>
#include <QPainter>
void WebSocketView::processTextMessage(QString message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (m_debug)
        qDebug() << "Message received:" << message;
    if (pClient) {

        QBuffer b;
        QSvgGenerator p;
        p.setOutputDevice(&b);
        p.setSize(QSize(1024,768));
        p.setViewBox(QRect(0,0,1024,768));
        QPainter painter;
        painter.begin(&p);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        m_scene->render(&painter);
        painter.end();

        pClient->sendTextMessage(b.buffer());
    }
}

void WebSocketView::processBinaryMessage(QByteArray message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (m_debug)
        qDebug() << "Binary Message received:" << message;
    if (pClient) {
        pClient->sendBinaryMessage(message);
    }
}

void WebSocketView::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (m_debug)
        qDebug() << "socketDisconnected:" << pClient;
    if (pClient) {
        m_clients.removeAll(pClient);
        pClient->deleteLater();
    }
}
