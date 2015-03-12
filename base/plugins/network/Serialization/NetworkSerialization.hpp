#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>


#include "../Repartition/session/Session.h"
struct NetworkMessage
{
        friend QDataStream& operator<<(QDataStream& s, const NetworkMessage& m)
        {
            s << m.address << m.sessionId << m.clientId << m.data;
            return s;
        }

        friend QDataStream& operator>>(QDataStream& s, NetworkMessage& m)
        {
            s >> m.address >> m.sessionId >> m.clientId >> m.data;
            return s;
        }

        NetworkMessage() = default;
        NetworkMessage(const NetworkMessage&) = default;
        NetworkMessage(NetworkMessage&&) = default;
        ~NetworkMessage() = default;

        NetworkMessage(Session& s, QString&& addr, QByteArray&& data):
            address{addr},
            sessionId{s.getId()},
            clientId{s.getClient().getId()},
            data{data}
        {
        }

        QString address;
        int sessionId;
        int clientId;
        QByteArray data;
};

class MessageMapper
{
    public:
        void addHandler(QString addr, std::function<void(QByteArray)> fun)
        {
            m_handlers[addr] = fun;
        }

        void map(QString addr, QByteArray data)
        {
            m_handlers[addr](data);
        }

        QList<QString> addresses() const
        {
            return m_handlers.keys();
        }

    private:
        QMap<QString, std::function<void(QByteArray)>> m_handlers;
};


class MessageValidator
{
    public:
        MessageValidator(Session& s, MessageMapper& map):
            m_session{s},
            m_mapper{map}
        {

        }

        bool validate(NetworkMessage m)
        {
            return  m_mapper.addresses().contains(m.address)
                 && m_session.getId() == m.sessionId
                 && m_session.getClient().getId() == m.clientId;
        }

    private:
        Session& m_session;
        MessageMapper& m_mapper;
};


// Utilisé par le serveur lorsque le client se connecte :
// le client a un NetworkSerializationServer qui tourne
// et le serveur écrit dedans avec le NetworkSerializationSocket
class NetworkSerializationSocket : public QObject
{
        Q_OBJECT
    public:
        NetworkSerializationSocket(QTcpSocket* sock, QObject* parent);
        NetworkSerializationSocket(QString ip, int port, QObject* parent);

        void sendMessage(NetworkMessage);

    signals:
        void messageReceived(NetworkMessage);

    private:
        void init();
        QTcpSocket* m_socket{};
};


class NetworkSerializationServer : public QObject
{
        Q_OBJECT
    public:
        NetworkSerializationServer(int port, QObject* parent);

    signals:
        void newSocket(QTcpSocket* sock);

    public slots:
        void acceptConnection();

    private:
        QTcpServer* m_tcpServer{};
};

