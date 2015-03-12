#pragma once
#include "ClientSession.hpp"
#include "../client/RemoteClient.hpp"

class ClientSessionBuilder : public QObject
{
        Q_OBJECT
    public:
        ClientSessionBuilder(QString ip, int port)
        {
            m_mastersocket = new NetworkSocket(ip, port, nullptr);
            connect(m_mastersocket, &NetworkSocket::messageReceived,
                    this, &ClientSessionBuilder::on_messageReceived);
        }

        void doConnection()
        {
            // Todo only call this if the socket is ready.
            NetworkMessage askId;
            askId.address = "/session/askNewId";
            m_mastersocket->sendMessage(askId);
        }

        ClientSession* builtSession() const
        {
            return m_session;
        }

        QByteArray documentData() const
        {
            return m_documentData;
        }

    public slots:
        void on_messageReceived(NetworkMessage m)
        {
            if(m.address == "/session/idOffer")
            {
                m_sessionId.setVal(m.sessionId); // The session offered
                m_masterId.setVal(m.clientId); // Message is from the master
                QDataStream s(m.data);
                s >> m_clientId; // The offered client id

                // TODO ask with additional data
                NetworkMessage join;
                join.address = "/session/join";
                join.clientId = m_clientId;
                join.sessionId = m.sessionId;

                m_mastersocket->sendMessage(join);
            }
            else if(m.address == "/session/document")
            {
                m_session = new ClientSession(new RemoteClient(m_mastersocket, m_masterId),
                                              new LocalClient(m_clientId),
                                              m_sessionId,
                                              nullptr);

                m_documentData = m.data;

                emit sessionBuilt();
            }
        }

    signals:
        void sessionBuilt();

    private:
        id_type<Client> m_masterId, m_clientId;
        id_type<Session> m_sessionId;
        NetworkSocket* m_mastersocket{};

        QByteArray m_documentData;

        ClientSession* m_session{};

};
