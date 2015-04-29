#pragma once
#include <QObject>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Serialization/NetworkMessage.hpp>

class Client;
class Session;
class ClientSession;
class NetworkSocket;
class ClientSessionBuilder : public QObject
{
        Q_OBJECT
    public:
        ClientSessionBuilder(QString ip, int port);

        void initiateConnection();
        ClientSession* builtSession() const;
        QByteArray documentData() const;
        const QList<QPair<QPair<QString, QString>, QByteArray> >& commandStackData() const;

    public slots:
        void on_messageReceived(const NetworkMessage& m);

    signals:
        void sessionReady(ClientSessionBuilder*, ClientSession*);

    private:
        QString m_clientName{"A Client"};
        id_type<Client> m_masterId, m_clientId;
        id_type<Session> m_sessionId;
        NetworkSocket* m_mastersocket{};


        QList<QPair <QPair <QString,QString>, QByteArray> > m_commandStack;
        QByteArray m_documentData;

        ClientSession* m_session{};
};
