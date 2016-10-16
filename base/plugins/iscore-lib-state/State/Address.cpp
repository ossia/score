#include <algorithm>

#include <State/Address.hpp>
#include <iscore/tools/Todo.hpp>
#include <QStringBuilder>
#include <State/Expression.hpp>
#include <ossia/network/base/name_validation.hpp>
#include <iscore/tools/std/Algorithms.hpp>
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
namespace State
{
Address::Address(const Address& other) noexcept :
    device{other.device},
    path{other.path}
{

}

Address& Address::operator=(const Address& other) noexcept
{
    device = other.device;
    path = other.path;
    return *this;
}

Address& Address::operator=(Address&& other) noexcept
{
    device = std::move(other.device);
    path = std::move(other.path);
    return *this;
}

Address::Address(QString d, QStringList p) noexcept :
    device{std::move(d)},
    path{std::move(p)}
{

}

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

bool Address::operator==(const Address& a) const
{
    return device == a.device && path == a.path;
}

bool Address::operator!=(const Address& a) const
{
    return !(*this == a);
}



AddressAccessor::AddressAccessor(const AddressAccessor& other) noexcept :
    address{other.address},
    qualifiers{other.qualifiers}
{

}

AddressAccessor::AddressAccessor(AddressAccessor&& other) noexcept :
    address{std::move(other.address)},
    qualifiers{std::move(other.qualifiers)}
{

}

AddressAccessor& AddressAccessor::operator=(const AddressAccessor& other) noexcept
{
    address = other.address;
    qualifiers = other.qualifiers;
    return *this;
}

AddressAccessor& AddressAccessor::operator=(AddressAccessor&& other) noexcept
{
    address = std::move(other.address);
    qualifiers = std::move(other.qualifiers);
    return *this;
}

AddressAccessor::AddressAccessor(Address a) noexcept :
    address{std::move(a)} { }

AddressAccessor::AddressAccessor(Address a, const AccessorVector& v) noexcept :
    address{std::move(a)},
    qualifiers{v, {}}
{

}

AddressAccessor& AddressAccessor::operator=(const Address& a)
{
    address = a;
    qualifiers.accessors.clear();
    return *this;
}

AddressAccessor& AddressAccessor::operator=(Address&& a)
{
    address = std::move(a);
    qualifiers.accessors.clear();
    return *this;
}

QString AddressAccessor::toString() const
{
    return address.toString() + State::toString(qualifiers);
}

QString AddressAccessor::toShortString() const
{
    return address.toShortString() + State::toString(qualifiers);
}

optional<AddressAccessor> AddressAccessor::fromString(
        const QString& str)
{
    return parseAddressAccessor(str);
}

bool AddressAccessor::operator==(const AddressAccessor& other) const
{
    return address == other.address && qualifiers == other.qualifiers;
}

bool AddressAccessor::operator!=(const AddressAccessor& a) const
{
    return !(*this == a);
}

QString AddressAccessorHead::toString() const
{
    return name + State::toString(qualifiers);
}


QDebug operator<<(QDebug d, const State::Address& a)
{
    d.noquote().nospace() << a.device % ":/" % a.path.join('/');
    return d;
}

QDebug operator<<(QDebug d, const State::AccessorVector& a)
{
    if(!a.empty())
    {
        QString str;

        for(auto acc : a)
        {
            str += "[" % QString::number(acc) % "]";
        }

        d.noquote().nospace() << str;
    }

    return d;
}

QDebug operator<<(QDebug d, const ossia::destination_qualifiers& a)
{
    d.noquote().nospace() << a.accessors;
    return d;
}

QDebug operator<<(QDebug d, const State::AddressAccessor& a)
{
    d.noquote().nospace() << a.address << a.qualifiers;
    return d;
}

QDebug operator<<(QDebug d, const State::AddressAccessorHead& a)
{
    d.noquote().nospace() << a.name << a.qualifiers;
    return d;
}


QStringList stringList(const Address& addr)
{
  return QStringList{} << addr.device << addr.path;
}

QString toString(const ossia::destination_qualifiers& qualifiers)
{
  QString str;
  if(qualifiers.unit)
  {
    auto unit_text = QString::fromStdString(ossia::get_pretty_unit_text(qualifiers.unit));
    if(!qualifiers.accessors.empty())
    {
      char c = ossia::get_unit_accessor(qualifiers.unit, qualifiers.accessors[0]);
      if(c != 0)
      {
        unit_text += '.';
        unit_text += c;
      }
    }

    str += "[" % std::move(unit_text) % "]";
  }
  else
  {
    for(auto acc : qualifiers.accessors)
    {
        str += "[" % QString::number(acc) % "]";
    }
  }
  return str;
}

}
