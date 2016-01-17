#pragma once
#include <QByteArray>
#include <QString>
#include <QMetaType>

class QDataStream;
namespace Network
{

class Session;

struct NetworkMessage
{
        friend QDataStream& operator<<(QDataStream& s, const NetworkMessage& m);
        friend QDataStream& operator>>(QDataStream& s, NetworkMessage& m);

        NetworkMessage() = default;
        NetworkMessage(Session& s, QString&& addr, QByteArray&& data);

        QString address;
        int sessionId{};
        int clientId{};
        QByteArray data;
};
}

Q_DECLARE_METATYPE(Network::NetworkMessage)
