#pragma once
#include <QVariant>
#include <QObject>
#include <QStringList>
#include <State/Address.hpp>
// TODO put in namespace

struct Message
{
    friend QDataStream& operator<<(QDataStream& s, const Message& m)
    {
        s << m.address << m.value;
        return s;
    }

    friend QDataStream& operator>>(QDataStream& s, Message& m)
    {
        s >> m.address >> m.value;
        return s;
    }

    Message() = default;
    Message(const Message&) = default;
    Message(Message&&) = default;
    Message& operator=(const Message&) = default;
    Message& operator=(Message&&) = default;

    bool operator==(const Message& m) const
    {
        return address == m.address && value == m.value;
    }
    bool operator!=(const Message& m) const
    {
        return address != m.address && value != m.value;
    }

    bool operator<(const Message& m) const
    {
        return false;
    }

    QString toString() const
    { return address.toString() + " " + value.toString(); }

    Address address;
    QVariant value;
};

using MessageList = QList<Message>;
inline bool operator<(const MessageList&, const MessageList&)
{
    return false;
}

Q_DECLARE_METATYPE(Message)
Q_DECLARE_METATYPE(MessageList)
