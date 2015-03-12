#pragma once
#include "ClientSession.h"
#include "../client/RemoteClient.h"

class ClientSessionBuilder : public QObject
{
        Q_OBJECT
    public:
        ClientSessionBuilder(QString ip, int port)
        {
            m_mastersocket = new NetworkSocket(ip, port, nullptr);
            connect(m_mastersocket, SIGNAL(messageReceived(NetworkMessage)),
                    this, SLOT(on_messageReceived(NetworkMessage)));

            NetworkMessage askId;
            askId.address = "/session/askNewId";

            m_mastersocket->sendMessage(askId);
        }

    public slots:
        void on_messageReceived(NetworkMessage m)
        {
            if(m.address == "/session/idOffer")
            {
                // Here (and here only) the clientId is used to transmit the new client id.
                sessionId = m.sessionId;
                clientId = m.clientId;

                NetworkMessage join;
                join.address = "/session/join";
                join.clientId = m.clientId;
                join.sessionId = m.sessionId;

                m_mastersocket->sendMessage(join);
            }
            else if(m.address == "/session/document")
            {
                //RemoteClient* master = new RemoteClient;
            }
        }

    private:
        int clientId{}, sessionId{};
        NetworkSocket* m_mastersocket;

};
