#include <ossia/editor/dataspace/dataspace.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <QDataStream>
#include <QJsonArray>
#include <QJsonValue>
#include <QtGlobal>
#include <State/ValueConversion.hpp>
#include <State/ValueSerialization.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/serialization/VariantSerialization.hpp>

#include <State/Value.hpp>
#include <iscore_lib_state_export.h>
JSON_METADATA(ossia::Impulse, "Impulse")
JSON_METADATA(int32_t, "Int")
JSON_METADATA(char, "Char")
JSON_METADATA(bool, "Bool")
JSON_METADATA(float, "Float")
JSON_METADATA(ossia::Vec2f, "Vec2f")
JSON_METADATA(ossia::Vec3f, "Vec3f")
JSON_METADATA(ossia::Vec4f, "Vec4f")
JSON_METADATA(std::vector<ossia::value>, "Tuple")
JSON_METADATA(std::string, "String")
JSON_METADATA(ossia::value, "Generic")
JSON_METADATA(ossia::net::domain_base<ossia::Impulse>, "Impulse")
JSON_METADATA(ossia::net::domain_base<int32_t>, "Int")
JSON_METADATA(ossia::net::domain_base<char>, "Char")
JSON_METADATA(ossia::net::domain_base<bool>, "Bool")
JSON_METADATA(ossia::net::domain_base<float>, "Float")
JSON_METADATA(ossia::net::domain_base<ossia::Vec2f>, "Vec2f")
JSON_METADATA(ossia::net::domain_base<ossia::Vec3f>, "Vec3f")
JSON_METADATA(ossia::net::domain_base<ossia::Vec4f>, "Vec4f")
JSON_METADATA(ossia::net::domain_base<std::vector<ossia::value>>, "Tuple")
JSON_METADATA(ossia::net::domain_base<std::string>, "String")
JSON_METADATA(ossia::net::domain_base<ossia::value>, "Generic")

ISCORE_DECL_VALUE_TYPE(int)
ISCORE_DECL_VALUE_TYPE(float)
ISCORE_DECL_VALUE_TYPE(bool)
ISCORE_DECL_VALUE_TYPE(char)
ISCORE_DECL_VALUE_TYPE(ossia::Impulse)
ISCORE_DECL_VALUE_TYPE(std::string)
ISCORE_DECL_VALUE_TYPE(ossia::Vec2f)
ISCORE_DECL_VALUE_TYPE(ossia::Vec3f)
ISCORE_DECL_VALUE_TYPE(ossia::Vec4f)

template <>
template <>
void VariantDataStreamSerializer<ossia::value_variant_type>::
    perform<ossia::Destination>()
{
  return; // How could we serialize destination ?
}

template <>
template <>
void VariantDataStreamDeserializer<ossia::value_variant_type>::
    perform<ossia::Destination>()
{
  i++;
  return; // How could we deserialize destination ?
}

template <>
template <>
void VariantJSONSerializer<ossia::value_variant_type>::
    perform<ossia::Destination>()
{
  return; // How could we serialize destination ?
}

template <>
template <>
void VariantJSONDeserializer<ossia::value_variant_type>::
    perform<ossia::Destination>()
{
  return; // How could we deserialize destination ?
}

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const ossia::value& n)
{
  readFrom((const ossia::value_variant_type&)n.v);
}

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::writeTo(ossia::value& n)
{
  writeTo((ossia::value_variant_type&)n.v);
}

DataStreamInput& operator<<(DataStreamInput& stream, const ossia::value& obj)
{
  DataStreamReader reader{stream.stream.device()};
  reader.readFrom(obj);
  return stream;
}

DataStreamOutput& operator>>(DataStreamOutput& stream, ossia::value& obj)
{
  DataStreamWriter writer{stream.stream.device()};
  writer.writeTo(obj);

  return stream;
}

template <typename T>
struct TSerializer<DataStream, ossia::net::domain_base<T>>
{
  using domain_t = ossia::net::domain_base<T>;
  static void readFrom(DataStream::Serializer& s, const domain_t& domain)
  {
    // Min
    {
      bool min_b{domain.min};
      s.stream() << min_b;
      if (min_b)
        s.stream() << *domain.min;
    }

    // Max
    {
      bool max_b{domain.max};
      s.stream() << max_b;
      if (max_b)
        s.stream() << *domain.max;
    }

    // Values
    {
      s.stream() << (int32_t)domain.values.size();
      for (auto& val : domain.values)
        s.stream() << val;
    }

    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, domain_t& domain)
  {
    {
      bool min_b;
      s.stream() >> min_b;
      if (min_b)
      {
        typename domain_t::value_type v;
        s.stream() >> v;
        domain.min = std::move(v);
      }
    }

    {
      bool max_b;
      s.stream() >> max_b;
      if (max_b)
      {
        typename domain_t::value_type v;
        s.stream() >> v;
        domain.max = std::move(v);
      }
    }

    {
      int32_t count;
      s.stream() >> count;
      for (int i = 0; i < count; i++)
      {
        typename domain_t::value_type v;
        s.stream() >> v;
        domain.values.insert(v);
      }
    }

    s.checkDelimiter();
  }
};

template <>
struct TSerializer<DataStream, ossia::net::domain_base<std::string>>
{
  using domain_t = ossia::net::domain_base<std::string>;
  static void readFrom(DataStream::Serializer& s, const domain_t& domain)
  {
    // Values
    {
      s.stream() << (int32_t)domain.values.size();
      for (auto& val : domain.values)
        s.stream() << val;
    }

    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, domain_t& domain)
  {
    {
      int32_t count;
      s.stream() >> count;
      for (int i = 0; i < count; i++)
      {
        std::string v;
        s.stream() >> v;
        domain.values.insert(v);
      }
    }

    s.checkDelimiter();
  }
};

template <>
struct TSerializer<DataStream, ossia::net::domain_base<ossia::Impulse>>
{
  using domain_t = ossia::net::domain_base<ossia::Impulse>;
  static void readFrom(DataStream::Serializer& s, const domain_t& domain)
  {
    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, domain_t& domain)
  {
    s.checkDelimiter();
  }
};


template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const ossia::net::domain& n)
{
  readFrom((const ossia::net::domain_base_variant&)n);
}


template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::writeTo(ossia::net::domain& n)
{
  writeTo((ossia::net::domain_base_variant&)n);
}

/// JSON ///
template <>
ossia::Impulse fromJsonValue<ossia::Impulse>(const QJsonValueRef& obj)
{
  return {};
}
template <>
std::array<float, 2>
fromJsonValue<std::array<float, 2>>(const QJsonValueRef& obj)
{
  std::array<float, 2> v;
  auto arr = obj.toArray();
  const std::size_t N = std::min(2, arr.size());
  for (std::size_t i = 0; i < N; i++)
    v[i] = arr[i].toDouble();

  return v;
}
template <>
std::array<float, 3>
fromJsonValue<std::array<float, 3>>(const QJsonValueRef& obj)
{
  std::array<float, 3> v;
  auto arr = obj.toArray();
  const std::size_t N = std::min(3, arr.size());
  for (std::size_t i = 0; i < N; i++)
    v[i] = arr[i].toDouble();

  return v;
}
template <>
std::array<float, 4>
fromJsonValue<std::array<float, 4>>(const QJsonValueRef& obj)
{
  std::array<float, 4> v;
  auto arr = obj.toArray();
  const std::size_t N = std::min(4, arr.size());
  for (std::size_t i = 0; i < N; i++)
    v[i] = arr[i].toDouble();

  return v;
}
template <>
std::array<float, 2> fromJsonValue<std::array<float, 2>>(const QJsonValue& obj)
{
  std::array<float, 2> v;
  auto arr = obj.toArray();
  const std::size_t N = std::min(2, arr.size());
  for (std::size_t i = 0; i < N; i++)
    v[i] = arr[i].toDouble();

  return v;
}
template <>
std::array<float, 3> fromJsonValue<std::array<float, 3>>(const QJsonValue& obj)
{
  std::array<float, 3> v;
  auto arr = obj.toArray();
  const std::size_t N = std::min(3, arr.size());
  for (std::size_t i = 0; i < N; i++)
    v[i] = arr[i].toDouble();

  return v;
}
template <>
std::array<float, 4> fromJsonValue<std::array<float, 4>>(const QJsonValue& obj)
{
  std::array<float, 4> v;
  auto arr = obj.toArray();
  const std::size_t N = std::min(4, arr.size());
  for (std::size_t i = 0; i < N; i++)
    v[i] = arr[i].toDouble();

  return v;
}
template <typename T>
QJsonArray toJsonArray(const boost::container::flat_set<T>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    arr.push_back(toJsonValue(v));
  return arr;
}

QJsonArray toJsonArray(const std::vector<ossia::value>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    arr.push_back(toJsonValue(v));
  return arr;
}

void fromJsonArray(const QJsonArray& arr, std::vector<ossia::value>& array)
{
  array.reserve(arr.size());
  for (const auto& val : arr)
  {
    array.push_back(fromJsonValue<ossia::value>(val));
  }
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::readFrom(const ossia::value& n);

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::writeTo(ossia::value& n);

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::readFrom(const ossia::value& n);
template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::writeTo(ossia::value& n);

template <>
struct TSerializer<JSONObject, std::vector<ossia::value>>
{
  static void
  readFrom(JSONObject::Serializer& s, const std::vector<ossia::value>& vec)
  {
    s.obj[s.strings.Values] = toJsonArray(vec);
  }

  static void
  writeTo(JSONObject::Deserializer& s, std::vector<ossia::value>& vec)
  {
    fromJsonArray(s.obj[s.strings.Values].toArray(), vec);
  }
};

template <>
struct TSerializer<JSONValue, std::vector<ossia::value>>
{
  static void
  readFrom(JSONValue::Serializer& s, const std::vector<ossia::value>& vec)
  {
    s.val = toJsonArray(vec);
  }

  static void
  writeTo(JSONValue::Deserializer& s, std::vector<ossia::value>& vec)
  {
    fromJsonArray(s.val.toArray(), vec);
  }
};

template <>
struct TSerializer<JSONValue, std::string>
{
  static void readFrom(JSONValue::Serializer& s, const std::string& v)
  {
    s.val = QString::fromStdString(v);
  }

  static void writeTo(JSONValue::Deserializer& s, std::string& val)
  {
    val = s.val.toString().toStdString();
  }
};

template <typename T>
struct TSerializer<JSONObject, ossia::net::domain_base<T>>
{
  using domain_t = ossia::net::domain_base<T>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
    if (domain.min)
      s.obj[s.strings.Min] = toJsonValue(*domain.min);
    if (domain.max)
      s.obj[s.strings.Max] = toJsonValue(*domain.max);
    if (!domain.values.empty())
      s.obj[s.strings.Values] = toJsonArray(domain.values);
  }

  static void writeTo(const JSONObject::Deserializer& s, domain_t& domain)
  {
    using val_t = typename domain_t::value_type;
    // OPTIMIZEME there should be something in boost
    // to get multiple iterators from multiple keys in one pass...
    auto it_min = s.obj.constFind(s.strings.Min);
    auto it_max = s.obj.constFind(s.strings.Max);
    auto it_values = s.obj.constFind(s.strings.Values);
    if (it_min != s.obj.constEnd())
    {
      domain.min = fromJsonValue<val_t>(*it_min);
    }
    if (it_max != s.obj.constEnd())
    {
      domain.max = fromJsonValue<val_t>(*it_max);
    }
    if (it_values != s.obj.constEnd())
    {
      const auto arr = it_values->toArray();
      for (const auto& v : arr)
      {
        domain.values.insert(fromJsonValue<val_t>(v));
      }
    }
  }
};

template <>
struct TSerializer<JSONObject, ossia::net::domain_base<std::string>>
{
  using domain_t = ossia::net::domain_base<std::string>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
    if (!domain.values.empty())
      s.obj[s.strings.Values] = toJsonArray(domain.values);
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain)
  {
    auto it_values = s.obj.constFind(s.strings.Values);
    if (it_values != s.obj.constEnd())
    {
      const auto arr = it_values->toArray();
      for (const auto& v : arr)
      {
        domain.values.insert(fromJsonValue<std::string>(v));
      }
    }
  }
};

template <>
struct TSerializer<JSONObject, ossia::net::domain_base<ossia::Impulse>>
{
  using domain_t = ossia::net::domain_base<ossia::Impulse>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain)
  {
  }
};


template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::readFrom(const ossia::net::domain& n)
{
  readFrom((const ossia::net::domain_base_variant&)n);
}


template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::writeTo(ossia::net::domain& n)
{
  writeTo((ossia::net::domain_base_variant&)n);
}


template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::readFrom(const ossia::value& n)
{
  readFrom((const ossia::value_variant_type&)n.v);
}


template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::writeTo(ossia::value& n)
{
  writeTo((ossia::value_variant_type&)n.v);
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::readFrom(const ossia::value& n)
{
  val = marshall<JSONObject>(n);
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::writeTo(ossia::value& n)
{
  n = unmarshall<ossia::value>(val.toObject());
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::readFrom(const ossia::Impulse& n)
{
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::writeTo(ossia::Impulse& n)
{
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::readFrom(const std::array<float, 2>& n)
{
  val = toJsonValue(n);
}
template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::readFrom(const std::array<float, 3>& n)
{
  val = toJsonValue(n);
}
template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::readFrom(const std::array<float, 4>& n)
{
  val = toJsonValue(n);
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::writeTo(std::array<float, 2>& n)
{
  fromJsonValue(val, n);
}
template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::writeTo(std::array<float, 3>& n)
{
  fromJsonValue(val, n);
}
template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::writeTo(std::array<float, 4>& n)
{
  fromJsonValue(val, n);
}


/// Instance bounds ///

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const ossia::net::instance_bounds& n)
{
  m_stream << n.min_instances << n.max_instances;
}


template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::writeTo(ossia::net::instance_bounds& n)
{
  m_stream >> n.min_instances >> n.max_instances;
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::readFrom(const ossia::net::instance_bounds& b)
{
  QJsonObject obj;
  obj[strings.Min] = b.min_instances;
  obj[strings.Max] = b.max_instances;
  val = std::move(obj);
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::writeTo(ossia::net::instance_bounds& n)
{
  const auto& obj = val.toObject();
  n.min_instances = obj[strings.Min].toInt();
  n.max_instances = obj[strings.Max].toInt();
}
