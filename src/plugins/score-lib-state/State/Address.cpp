// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <State/Address.hpp>
#include <State/AddressParser.hpp>
#include <State/Unit.hpp>
#include <State/UpdateAddress.hpp>

#include <score/tools/Debug.hpp>
#include <score/tools/ForEach.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash.hpp>
#include <ossia/network/base/name_validation.hpp>
#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

#include <QDebug>
#include <QStringBuilder>

#include <wobjectimpl.h>

W_GADGET_IMPL(State::DestinationQualifiers)
namespace State
{
DestinationQualifiers::DestinationQualifiers() noexcept
    : qualifiers{std::make_unique<ossia::destination_qualifiers>()}
{
}

DestinationQualifiers::DestinationQualifiers(const DestinationQualifiers& other)
    : qualifiers{std::make_unique<ossia::destination_qualifiers>(*other.qualifiers)}
{
}

DestinationQualifiers::DestinationQualifiers(DestinationQualifiers&& other) noexcept
    : qualifiers{std::move(other.qualifiers)}
{
  other.qualifiers = std::make_unique<ossia::destination_qualifiers>();
}

DestinationQualifiers&
DestinationQualifiers::operator=(const DestinationQualifiers& other)
{
  *qualifiers = *other.qualifiers;
  return *this;
}

DestinationQualifiers&
DestinationQualifiers::operator=(DestinationQualifiers&& other) noexcept
{
  *qualifiers = std::move(*other.qualifiers);
  other.qualifiers = std::make_unique<ossia::destination_qualifiers>();
  return *this;
}

DestinationQualifiers::~DestinationQualifiers() { }

DestinationQualifiers::DestinationQualifiers(const ossia::destination_qualifiers& other)
    : qualifiers{std::make_unique<ossia::destination_qualifiers>(other)}
{
}

DestinationQualifiers&
DestinationQualifiers::operator=(const ossia::destination_qualifiers& other)
{
  *qualifiers = other;
  return *this;
}

bool DestinationQualifiers::operator==(const DestinationQualifiers& other) const noexcept
{
  return *qualifiers == *other.qualifiers;
}

bool DestinationQualifiers::operator!=(const DestinationQualifiers& other) const noexcept
{
  return *qualifiers != *other.qualifiers;
}

const ossia::destination_qualifiers& DestinationQualifiers::get() const noexcept
{
  return *qualifiers;
}

ossia::destination_qualifiers& DestinationQualifiers::get() noexcept
{
  return *qualifiers;
}

DestinationQualifiers::operator const ossia::destination_qualifiers&() const noexcept
{
  return *qualifiers;
}

DestinationQualifiers::operator ossia::destination_qualifiers&() noexcept
{
  return *qualifiers;
}

Address::Address() noexcept = default;
Address::Address(Address&&) noexcept = default;

Address::Address(const Address& other) noexcept
    : device{other.device}
    , path{other.path}
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

Address::Address(QString d, QStringList p) noexcept
    : device{std::move(d)}
    , path{std::move(p)}
{
}

bool Address::validateString(const QString& str)
{
  auto firstcolon = str.indexOf(":");
  auto firstslash = str.indexOf("/");

  bool valid = str == QString(str.toUtf8()) && (firstcolon > 0)
               && (firstslash == (firstcolon + 1) && !str.contains("//"));

  QStringList path = str.split("/");
  if(path.empty())
    return false;

  if(path.first().count(':') > 1)
    return false;

  path.first().remove(":");

  Foreach(path, [&](const auto& fragment) { valid &= validateFragment(fragment); });

  return valid;
}

bool Address::validateFragment(const QString& s)
{
  return ossia::all_of(s, [](QChar c) {
    constexpr std::string_view pmatch_chars = "_~(): .*?,{}[]-";
    return c.isLetterOrNumber()
           || (pmatch_chars.find(c.toLatin1()) != std::string_view::npos);
  });
}

std::optional<Address> Address::fromString(const QString& str)
{
  if(!validateString(str))
    return {};

  QStringList path = str.split("/");
  SCORE_ASSERT(path.size() > 0);

  auto device = path.first().remove(":");
  path.removeFirst();        // Remove the device.
  if(path.first().isEmpty()) // case "device:/"
  {
    return State::Address{device, {}};
  }

  return State::Address{device, path};
}

QString Address::toString() const
{
  QString ad = toString_unsafe();
  if(!validateString(ad))
    ad.clear();

  return ad;
}

QString Address::toString_unsafe() const
{
  return device % ":/" % path.join("/");
}

QString Address::toShortString() const
{
  auto str = toString();
  return str.size() < 15 ? str : "..." % str.right(12);
}

bool Address::isSet() const noexcept
{
  return !device.isEmpty();
}

bool Address::operator==(const Address& a) const
{
  return device == a.device && path == a.path;
}

bool Address::operator!=(const Address& a) const
{
  return !(*this == a);
}

AddressAccessor::AddressAccessor() noexcept { }

AddressAccessor::AddressAccessor(const AddressAccessor& other) noexcept
    : address{other.address}
    , qualifiers{other.qualifiers}
{
}

AddressAccessor::AddressAccessor(AddressAccessor&& other) noexcept
    : address{std::move(other.address)}
    , qualifiers{std::move(other.qualifiers)}
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

AddressAccessor::AddressAccessor(Address a) noexcept
    : address{std::move(a)}
{
}

AddressAccessor::AddressAccessor(Address a, const AccessorVector& v) noexcept
    : address{std::move(a)}
    , qualifiers{ossia::destination_qualifiers{v, {}}}
{
}

AddressAccessor::AddressAccessor(
    Address a, const AccessorVector& v, const ossia::unit_t& u) noexcept
    : address{std::move(a)}
    , qualifiers{ossia::destination_qualifiers{v, u}}
{
}

AddressAccessor& AddressAccessor::operator=(const Address& a)
{
  address = a;
  qualifiers.get().accessors.clear();
  return *this;
}

AddressAccessor& AddressAccessor::operator=(Address&& a)
{
  address = std::move(a);
  qualifiers.get().accessors.clear();
  return *this;
}

QString AddressAccessor::toString() const
{
  const auto addr = address.toString();
  if(Q_UNLIKELY(addr.isEmpty()))
    return QLatin1String(":/") + State::toString(qualifiers);
  else
    return addr + State::toString(qualifiers);
}

QString AddressAccessor::toString_unsafe() const
{
  return address.toString_unsafe() + State::toString(qualifiers);
}

QString AddressAccessor::toShortString() const
{
  return address.toShortString() + State::toString(qualifiers);
}

bool AddressAccessor::isSet() const noexcept
{
  return address.isSet();
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

QDebug operator<<(QDebug d, const State::DestinationQualifiers& val)
{
  auto& a = val.get();
  d.noquote().nospace() << a.accessors;
  if(a.unit)
    d << QString::fromStdString(std::string(ossia::get_pretty_unit_text(a.unit)));

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
  QStringList l;
  l.reserve(1 + addr.path.size());
  l.append(addr.device);
  l.append(addr.path);
  return l;
}

QString toString(const ossia::destination_qualifiers& qualifiers)
{
  QString str;
  str.reserve(16);
  if(qualifiers.unit)
  {
    QString unit_text = State::prettyUnitText(qualifiers.unit);
    //  = QString::fromStdString(ossia::get_pretty_unit_text(qualifiers.unit));
    if(!qualifiers.accessors.empty())
    {
      char c = ossia::get_unit_accessor(qualifiers.unit, qualifiers.accessors[0]);
      if(c != 0)
      {
        unit_text += '.';
        unit_text += c;
      }
    }

    str += "@[";
    str += std::move(unit_text);
    str += "]";
  }
  else
  {
    if(!qualifiers.accessors.empty())
    {
      str += '@';
      for(auto acc : qualifiers.accessors)
      {
        str += "[";
        str += QString::number(acc);
        str += "]";
      }
    }
  }
  return str;
}

std::optional<State::AddressAccessor>
onUpdatableAddress(const State::AddressAccessor& current, const QMimeData& mime)
{
  if(mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();
    if(ml.empty())
      return {};
    auto& newAddr = ml[0].address;

    if(newAddr == current)
      return {};

    // TODO do we want that ? we may want to clean the address...
    if(newAddr.address.path.isEmpty())
      return {};

    return newAddr;
  }
  return {};
}

std::optional<State::Address>
onUpdatableAddress(const State::Address& current, const QMimeData& mime)
{
  if(mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();
    if(ml.empty())
      return {};
    auto& newAddr = ml[0].address;

    if(newAddr.address == current)
      return {};

    // TODO do we want that ? we may want to clean the address...
    if(newAddr.address.path.isEmpty())
      return {};

    return newAddr.address;
  }
  return {};
}

bool addressIsChildOf(
    const State::Address& root, const State::Address& maybe_child) noexcept
{
  if(root.device != maybe_child.device)
    return false;

  if(root.path.length() > maybe_child.path.length())
    return false;

  for(int i = 0; i < root.path.length(); i++)
    if(root.path[i] != maybe_child.path[i])
      return false;

  return true;
}

State::Address
replaceAddressPart(const State::Address& old, const State::Address& replacement) noexcept
{
  State::Address ret;
  ret = {replacement.device, old.path};

  for(int i = 0, N = std::min(ret.path.size(), replacement.path.size()); i < N; i++)
  {
    ret.path[i] = replacement.path[i];
  }

  return ret;
}

State::AddressAccessor replaceAddressPart(
    const State::AddressAccessor& old, const State::Address& replacement) noexcept
{
  State::AddressAccessor ret;
  ret.address = {replacement.device, old.address.path};
  ret.qualifiers = old.qualifiers;

  for(int i = 0, N = std::min(ret.address.path.size(), replacement.path.size()); i < N;
      i++)
  {
    ret.address.path[i] = replacement.path[i];
  }

  return ret;
}

void rerootAddress(
    State::Address& current, const State::Address& oldRoot,
    const State::Address& newRoot)
{
  if(State::addressIsChildOf(oldRoot, current))
    current = replaceAddressPart(current, newRoot);
}

}

std::size_t std::hash<State::Address>::operator()(const State::Address& k) const
{
  std::size_t r = qHash(k.device);
  ossia::hash_combine(r, qHashRange(k.path.begin(), k.path.end()));
  return r;
}

namespace std
{
template <>
struct hash<ossia::unit_variant>
{
  std::size_t operator()(const ossia::unit_variant& k) const
  {
    std::size_t seed = 0;
    ossia::hash_combine(seed, k.which());
    if(k)
    {
      ossia::apply_nonnull(
          [&](const auto& ds) { ossia::hash_combine(seed, ds.which()); }, k);
    }
    return seed;
  }
};
}

std::size_t
std::hash<State::AddressAccessor>::operator()(const State::AddressAccessor& k) const
{
  std::size_t seed = 0;
  ossia::hash_combine(seed, k.address);
  auto& qual = k.qualifiers.get();
  for(auto& q : qual.accessors)
  {
    ossia::hash_combine(seed, q);
  }
  ossia::hash_combine(seed, qual.unit.v);
  return seed;
}

std::optional<State::Address> State::parseAddress(const QString& str)
{
  auto input = str.toStdString();
  auto f(std::begin(input)), l(std::end(input));
  auto p = std::make_unique<Address_parser<decltype(f)>>();
  try
  {
    State::Address result;
    bool ok = qi::phrase_parse(f, l, *p, qi::standard::space, result);

    if(!ok)
    {
      return {};
    }

    return result;
  }
  catch(const qi::expectation_failure<decltype(f)>& e)
  {
    // SCORE_BREAKPOINT;
    return {};
  }
  catch(...)
  {
    // SCORE_BREAKPOINT;
    return {};
  }
}

namespace
{
bool validatePatternCharacters(const QString& str)
{
  const auto chars = QStringLiteral(
      "abcdefghijklmnopqrstuvwxyz"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "0123456789"
      "_~(): .*?,{}[]-@/");
  for(const auto& c : str)
  {
    if(Q_UNLIKELY(!chars.contains(c)))
      return false;
  }
  return true;
}
}
std::optional<State::AddressAccessor> State::parseAddressAccessor(const QString& str)
{
  if(!validatePatternCharacters(str))
    return std::nullopt;

  auto input = str.toStdString();
  auto f(std::begin(input)), l(std::end(input));
  auto p = std::make_unique<AddressAccessor_parser<decltype(f)>>();
  try
  {
    State::AddressAccessor result;
    bool ok = qi::phrase_parse(f, l, *p, qi::standard::space, result);

    if(ok)
    {
      return result;
    }
    else
    {
      // We try to get an address instead.
      std::optional<State::Address> res = State::parseAddress(str);
      if(res)
      {
        result.address = (*res);
        result.qualifiers.get().accessors.clear();

        return result;
      }
      else
      {
        return {};
      }
    }
  }
  catch(const qi::expectation_failure<decltype(f)>& e)
  {
    // SCORE_BREAKPOINT;
    return {};
  }
  catch(...)
  {
    // SCORE_BREAKPOINT;
    return {};
  }
}
