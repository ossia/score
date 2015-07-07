#include "Address.hpp"
#include <QDataStream>
namespace iscore
{
bool Address::validateString(const QString &str)
{
    auto firstcolon = str.indexOf(":");
    auto firstslash = str.indexOf("/");
    return firstcolon > 0 && firstslash > firstcolon;
}


Address Address::fromString(const QString &str)
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

QDataStream& operator<<(QDataStream &s, const iscore::Address &a)
{
    s << a.device << a.path;
    return s;
}

QDataStream& operator>>(QDataStream &s, iscore::Address &a)
{
    s >> a.device >> a.path;
    return s;
}
}
