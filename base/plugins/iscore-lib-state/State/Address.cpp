#include "Address.hpp"
#include <QDataStream>
namespace iscore
{
    bool Address::validateString(const QString &str)
    {
        auto firstcolon = str.indexOf(":");
        auto firstslash = str.indexOf("/");
        return str == QString(str.toUtf8())
                && (firstcolon > 0)
                && (firstslash == (firstcolon + 1)
                && !str.contains("//"));
    }


    Address Address::fromString(const QString &str)
    {
        if (!validateString( str))
            return {"", {""} };

        QStringList path = str.split("/");
        ISCORE_ASSERT(path.size() > 0);

        auto device = path.first().remove(":");
        path.removeFirst(); // Remove the device.
        if(path.first().isEmpty()) // case "device:/"
        {
            return {device, {}};
        }

        return {device, path};
    }

    QString Address::toString() const
    {
        QString ad =  device + ":/" + path.join("/");
        if (!validateString(ad))
            ad.clear();

        return ad;
    }
}
