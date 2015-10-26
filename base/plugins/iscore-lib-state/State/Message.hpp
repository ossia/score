#pragma once
#include <QObject>
#include <QStringList>
#include <State/Address.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
namespace iscore
{
/**
 * @brief The Message struct
 *
 * A message is an Address associated with a value :
 *
 *  aDevice:/aNode/anotherNode 2345
 *
 */
struct Message
{
    Message() = default;
    Message(const iscore::Address& addr, const iscore::Value& val):
        address(addr),
        value(val)
    { }

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
    { return address.toString() + " " + iscore::convert::toPrettyString(value); }

    friend QDebug operator<<(QDebug s, const iscore::Message& mess)
    {
        s << mess.toString();
        return s;
    }

    Address address;
    Value value;
};

using MessageList = QList<Message>;
inline bool operator<(const iscore::MessageList&, const iscore::MessageList&)
{
    return false;
}
}

Q_DECLARE_METATYPE(iscore::Message)
Q_DECLARE_METATYPE(iscore::MessageList)
