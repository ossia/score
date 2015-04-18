#pragma once
#include "../client/LocalClient.hpp"
#include "../client/RemoteClient.hpp"
#include <Serialization/NetworkMessage.hpp>

class MasterSession;
class MessageMapper;
class MessageValidator;

class Session : public IdentifiedObject<Session>
{
        Q_OBJECT
    public:
        Session(LocalClient* client,
                id_type<Session> id,
                QObject* parent = nullptr);


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

        const LocalClient& localClient() const
        {
            return *m_client;
        }

        const QList<RemoteClient*>& remoteClients() const
        {
            return m_remoteClients;
        }

        void addClient(RemoteClient* clt)
        {
            m_remoteClients.append(clt);
            emit clientsChanged();
        }

        NetworkMessage makeMessage(QString address);

        template<typename... Args>
        NetworkMessage makeMessage(QString address, Args&&... args)
        {
            NetworkMessage m;
            m.address = address;
            m.clientId = localClient().id().val().get();
            m.sessionId = id().val().get();

            impl_makeMessage(QDataStream{&m.data, QIODevice::WriteOnly}, std::forward<Args&&>(args)...);

            return m;
        }

    signals:
        void clientsChanged();

    public slots:
        void validateMessage(NetworkMessage m);

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
