#pragma once
#include "../client/LocalClient.hpp"
#include "../client/RemoteClient.hpp"
#include <Serialization/NetworkMessage.hpp>
#include <Serialization/MessageMapper.hpp>
#include <Serialization/MessageValidator.hpp>

class MasterSession;

class Session : public IdentifiedObject<Session>
{
        Q_OBJECT
    public:
        Session(LocalClient* client,
                id_type<Session> id,
                QObject* parent = nullptr):
            IdentifiedObject<Session>{id, "Session", parent},
            m_client{client},
            m_mapper{new MessageMapper},
            m_validator{new MessageValidator(*this, mapper())}
        {
        }


        MessageValidator& validator()
        {
            return *m_validator;
        }
        MessageMapper& mapper()
        {
            return *m_mapper;
        }

        LocalClient& localClient()
        {
            return *m_client;
        }

        QList<RemoteClient*>& remoteClients()
        {
            return m_remoteClients;
        }


        NetworkMessage makeMessage(QString address)
        {
            NetworkMessage m;
            m.address = address;
            m.clientId = localClient().id();
            m.sessionId = id();

            return m;
        }
        template<typename... Args>
        NetworkMessage makeMessage(QString address, Args&&... args)
        {
            NetworkMessage m;
            m.address = address;
            m.clientId = localClient().id();
            m.sessionId = id();

            impl_makeMessage(QDataStream{&m.data, QIODevice::WriteOnly}, std::forward<Args&&>(args)...);

            return m;
        }

    public slots:
        void validateMessage(NetworkMessage m)
        {
            if(validator().validate(m))
                mapper().map(m);
        }


    private:
        template<typename Arg>
        void impl_makeMessage(QDataStream&& s, Arg&& arg)
        {
            s << arg;
        }

        template<typename Arg, typename... Args>
        void impl_makeMessage(QDataStream&& s, Arg&& arg, Args&&... args)
        {
            impl_makeMessage(std::move(s << arg), std::forward<Args&&>(args)...);
        }


        LocalClient* m_client{};
        MessageMapper* m_mapper{};
        MessageValidator* m_validator{};
        QList<RemoteClient*> m_remoteClients;


};
