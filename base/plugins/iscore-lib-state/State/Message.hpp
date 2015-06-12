#pragma once
#include <QVariant>
#include <QObject>
#include <QStringList>
struct Address
{
        static bool validateString(const QString& str)
        {
            auto firstcolon = str.indexOf(":");
            auto firstslash = str.indexOf("/");
            return firstcolon > 0 && firstslash > firstcolon;
        }

        static Address fromString(const QString& str)
        {
            QStringList path = str.split("/");
            Q_ASSERT(path.size() > 0);

            auto device = path.first().remove(":");
            path.removeFirst(); // Remove the device.
            if(path.first().isEmpty()) // case "device:/"
            {
                return {device, {}};
            }

            return {device, path};
        }

        QString device;
        // Note : path is empty if address is root: "device:/"
        QStringList path;

        QString toString() const
        { return device + ":/" + path.join("/"); }

        bool operator==(const Address& a) const
        {
            return device == a.device && path == a.path;
        }
        bool operator!=(const Address& a) const
        {
            return !(*this == a);
        }

        friend QDataStream& operator<<(QDataStream& s, const Address& a)
        {
            s << a.device << a.path;
            return s;
        }

        friend QDataStream& operator>>(QDataStream& s, Address& a)
        {
            s >> a.device >> a.path;
            return s;
        }
};

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
