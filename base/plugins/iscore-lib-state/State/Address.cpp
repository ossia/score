#include <algorithm>

#include <State/Address.hpp>
#include <iscore/tools/Todo.hpp>
#include <QStringBuilder>
#include <State/Expression.hpp>
#include <ossia/network/base/name_validation.hpp>
#include <iscore/tools/std/Algorithms.hpp>
namespace State
{
bool Address::validateString(const QString &str)
{
    auto firstcolon = str.indexOf(":");
    auto firstslash = str.indexOf("/");

    bool valid = str == QString(str.toUtf8())
            && (firstcolon > 0)
            && (firstslash == (firstcolon + 1)
                && !str.contains("//"));

    QStringList path = str.split("/");
    valid &= !path.empty();

    path.first().remove(":");

    Foreach(path, [&] (const auto& fragment) {
        valid &= validateFragment(fragment);
    });

    return valid;
}

bool Address::validateFragment(const QString& s)
{
    return ossia::all_of(s, &ossia::net::is_valid_character_for_name<QChar>);
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

Address Address::rootAddress()
{
    return Address();
}

QString Address::toString() const
{
    QString ad = device % ":/" % path.join("/");
    if (!validateString(ad))
        ad.clear();

    return ad;
}

QString Address::toShortString() const
{
    auto str = toString();
    return str.size() < 15 ? str : "..." % str.right(12);
}

QString AddressAccessor::toString() const
{
    auto str = address.toString();
    for(auto acc : accessors)
    {
        str += "[" % QString::number(acc) % "]";
    }
    return str;
}

QString AddressAccessor::toShortString() const
{
    auto str = address.toShortString();
    for(auto acc : accessors)
    {
        str += "[" % QString::number(acc) % "]";
    }
    return str;
}

QString AddressAccessor::accessorsString() const
{
    QString str;
    for(auto acc : accessors)
    {
        str += "[" % QString::number(acc) % "]";
    }
    return str;
}

optional<AddressAccessor> AddressAccessor::fromString(
        const QString& str)
{
    return parseAddressAccessor(str);
}

QDebug operator<<(QDebug d, const State::Address& a)
{
    d.noquote().nospace() << a.device % ":/" % a.path.join('/');
    return d;
}
}
