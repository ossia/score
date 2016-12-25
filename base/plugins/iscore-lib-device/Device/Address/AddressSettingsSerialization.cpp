#include <QDataStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "AddressSettings.hpp"
#include <ossia/editor/dataspace/dataspace.hpp>
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <Device/Address/ClipMode.hpp>
#include <Device/Address/Domain.hpp>
#include <Device/Address/IOType.hpp>
#include <State/Value.hpp>


struct ISCORE_LIB_BASE_EXPORT any_serializer
{
  virtual ~any_serializer();
  virtual void apply(Serializer<DataStream>&, const boost::any&) = 0;
  virtual void apply(Serializer<JSONValue>&, const boost::any&) = 0;
  virtual void apply(Deserializer<DataStream>&, boost::any&) = 0;
  virtual void apply(Deserializer<JSONValue>&, boost::any&) = 0;
};

any_serializer::~any_serializer()
{

}

template<typename T>
struct any_serializer_t final : public any_serializer
{
  void apply(Serializer<DataStream>& s, const boost::any& val) override
  { return s.readFrom(boost::any_cast<T>(val)); }
  void apply(Serializer<JSONValue>& s, const boost::any& val) override
  { return s.readFrom(boost::any_cast<T>(val)); }
  void apply(Deserializer<DataStream>& s, boost::any& val) override
  { return s.writeTo(boost::any_cast<T>(val)); }
  void apply(Deserializer<JSONValue>& s, boost::any& val) override
  { return s.writeTo(boost::any_cast<T>(val)); }
};

using any_map = iscore::hash_map<std::string, boost::any>;
iscore::hash_map<std::string, std::unique_ptr<any_serializer>>& anySerializers()
{
  static
      iscore::hash_map<std::string, std::unique_ptr<any_serializer>> ser;
  return ser;
}

template<typename Vis, typename Any>
void apply(Vis& s, const std::string& key, Any& v)
{
  auto& ser = anySerializers();
  auto it = ser.find(key);
  if(it != ser.end())
  {
    it.value()->apply(s, v);
  }
  else
  {
    ISCORE_TODO;
  }
}

template<>
struct TSerializer<DataStream, void, any_map>
{
  static void readFrom(
      DataStream::Serializer& s,
      const any_map& obj)
  {
    auto& st = s.stream();

    st << (int32_t) obj.size();
    for(const auto& e : obj)
    {
      st << e.first;
      apply(s, e.first, e.second);
    }
  }

  static void writeTo(
      DataStream::Deserializer& s,
      any_map& obj)
  {
    auto& st = s.stream();
    int32_t n;
    st >> n;
    for(int i = 0; i < n; i++)
    {
      std::string key;
      boost::any value;
      st >> key;
      apply(s, key, value);
      obj.emplace(std::move(key), std::move(value));
    }
  }
};


template<>
struct TSerializer<JSONObject, any_map>
{
  static void readFrom(
      JSONObject::Serializer& s,
      const any_map& obj)
  {
    for(const auto& e : obj)
    {
      Serializer<JSONValue> v{};
      apply(v, e.first, e.second);
      s.m_obj[QString::fromStdString(e.first)] = v.val;
    }
  }

  static void writeTo(
      JSONObject::Deserializer& s,
      any_map& obj)
  {
    const QJsonObject& extended = s.m_obj;
    for(const auto& k : extended.keys())
    {
      Deserializer<JSONValue> v{extended[k]};
      auto key = k.toStdString();
      boost::any val;
      apply(v, key, val);
      obj[std::move(key)] = std::move(val);
    }
  }
};

template <>
struct is_template<any_map> : std::true_type
{
};


template <>
void Visitor<Reader<DataStream>>::readFrom(
    const Device::AddressSettingsCommon& n)
{
  m_stream << n.value << n.domain << n.ioType << n.clipMode << n.unit
           << n.repetitionFilter << n.extendedAttributes;
}

template <>
void Visitor<Writer<DataStream>>::writeTo(Device::AddressSettingsCommon& n)
{
  m_stream >> n.value >> n.domain >> n.ioType >> n.clipMode >> n.unit
      >> n.repetitionFilter >> n.extendedAttributes;
}

template <>
void Visitor<Reader<JSONObject>>::readFrom(
    const Device::AddressSettingsCommon& n)
{
  // Metadata
  m_obj[strings.ioType] = Device::IOTypeStringMap()[n.ioType];
  m_obj[strings.ClipMode] = Device::ClipModeStringMap()[n.clipMode];
  m_obj[strings.Unit]
      = QString::fromStdString(ossia::get_pretty_unit_text(n.unit));

  m_obj[strings.RepetitionFilter] = n.repetitionFilter;

  // Value, domain and type
  readFrom(n.value);
  m_obj[strings.Domain] = toJsonObject(n.domain);

  m_obj[strings.Extended] = toJsonObject(n.extendedAttributes);
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(Device::AddressSettingsCommon& n)
{
  n.ioType = Device::IOTypeStringMap().key(m_obj[strings.ioType].toString());
  n.clipMode
      = Device::ClipModeStringMap().key(m_obj[strings.ClipMode].toString());
  n.unit
      = ossia::parse_pretty_unit(m_obj[strings.Unit].toString().toStdString());

  n.repetitionFilter = m_obj[strings.RepetitionFilter].toBool();

  writeTo(n.value);
  n.domain = fromJsonObject<Device::Domain>(m_obj[strings.Domain].toObject());

  n.extendedAttributes = fromJsonObject<any_map>(m_obj[strings.Extended]);
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Reader<DataStream>>::readFrom(const Device::AddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  m_stream << n.name;

  insertDelimiter();
}
template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<DataStream>>::writeTo(Device::AddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  m_stream >> n.name;

  checkDelimiter();
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Reader<JSONObject>>::readFrom(const Device::AddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  m_obj[strings.Name] = n.name;
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(Device::AddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  n.name = m_obj[strings.Name].toString();
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Reader<DataStream>>::readFrom(const Device::FullAddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  m_stream << n.address;

  insertDelimiter();
}
template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<DataStream>>::writeTo(Device::FullAddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  m_stream >> n.address;

  checkDelimiter();
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Reader<JSONObject>>::readFrom(const Device::FullAddressSettings& n)
{
  readFrom(static_cast<const Device::AddressSettingsCommon&>(n));
  m_obj[strings.Address] = toJsonObject(n.address);
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(Device::FullAddressSettings& n)
{
  writeTo(static_cast<Device::AddressSettingsCommon&>(n));
  n.address = fromJsonObject<State::Address>(m_obj[strings.Address]);
}

template <>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<DataStream>>::readFrom(
    const Device::FullAddressAccessorSettings& n)
{
  m_stream << n.value << n.domain << n.ioType << n.clipMode
           << n.repetitionFilter << n.extendedAttributes << n.address;
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<DataStream>>::writeTo(Device::FullAddressAccessorSettings& n)
{
  m_stream >> n.value >> n.domain >> n.ioType >> n.clipMode
      >> n.repetitionFilter >> n.extendedAttributes >> n.address;
}

template <>
ISCORE_LIB_DEVICE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(
    const Device::FullAddressAccessorSettings& n)
{
  // Metadata
  m_obj[strings.ioType] = Device::IOTypeStringMap()[n.ioType];
  m_obj[strings.ClipMode] = Device::ClipModeStringMap()[n.clipMode];

  m_obj[strings.RepetitionFilter] = n.repetitionFilter;

  // Value, domain and type
  readFrom(n.value);
  m_obj[strings.Domain] = toJsonObject(n.domain);
  m_obj[strings.Extended] = toJsonObject(n.extendedAttributes);

  m_obj[strings.Address] = toJsonObject(n.address);
}

template <>
ISCORE_LIB_DEVICE_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(Device::FullAddressAccessorSettings& n)
{
  n.ioType = Device::IOTypeStringMap().key(m_obj[strings.ioType].toString());
  n.clipMode
      = Device::ClipModeStringMap().key(m_obj[strings.ClipMode].toString());

  n.repetitionFilter = m_obj[strings.RepetitionFilter].toBool();


  writeTo(n.value);

  n.domain = fromJsonObject<Device::Domain>(m_obj[strings.Domain].toObject());
  n.extendedAttributes = fromJsonObject<any_map>(m_obj[strings.Extended]);


  n.address = fromJsonObject<State::AddressAccessor>(m_obj[strings.Address]);
}

