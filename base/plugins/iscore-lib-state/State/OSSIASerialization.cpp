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
JSON_METADATA(ossia::impulse, "Impulse")
JSON_METADATA(int32_t, "Int")
JSON_METADATA(char, "Char")
JSON_METADATA(bool, "Bool")
JSON_METADATA(float, "Float")
JSON_METADATA(ossia::vec2f, "Vec2f")
JSON_METADATA(ossia::vec3f, "Vec3f")
JSON_METADATA(ossia::vec4f, "Vec4f")
JSON_METADATA(std::vector<ossia::value>, "Tuple")
JSON_METADATA(std::string, "String")
JSON_METADATA(ossia::value, "Generic")
JSON_METADATA(ossia::domain_base<ossia::impulse>, "Impulse")
JSON_METADATA(ossia::domain_base<int32_t>, "Int")
JSON_METADATA(ossia::domain_base<char>, "Char")
JSON_METADATA(ossia::domain_base<bool>, "Bool")
JSON_METADATA(ossia::domain_base<float>, "Float")
JSON_METADATA(ossia::vecf_domain<2>, "Vec2f")
JSON_METADATA(ossia::vecf_domain<3>, "Vec3f")
JSON_METADATA(ossia::vecf_domain<4>, "Vec4f")
JSON_METADATA(ossia::vector_domain, "Tuple")
JSON_METADATA(ossia::domain_base<std::string>, "String")
JSON_METADATA(ossia::domain_base<ossia::value>, "Generic")

ISCORE_DECL_VALUE_TYPE(int)
ISCORE_DECL_VALUE_TYPE(float)
ISCORE_DECL_VALUE_TYPE(bool)
ISCORE_DECL_VALUE_TYPE(char)
ISCORE_DECL_VALUE_TYPE(ossia::impulse)
ISCORE_DECL_VALUE_TYPE(std::string)
ISCORE_DECL_VALUE_TYPE(ossia::vec2f)
ISCORE_DECL_VALUE_TYPE(ossia::vec3f)
ISCORE_DECL_VALUE_TYPE(ossia::vec4f)

template<>
struct is_custom_serialized<ossia::vector_domain> : public std::true_type { };
template<std::size_t N>
struct is_custom_serialized<ossia::vecf_domain<N>> : public std::true_type { };
template<typename T, std::size_t N>
struct is_custom_serialized<std::array<T, N>> : public std::true_type { };

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
DataStreamWriter::write(ossia::value& n)
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
struct TSerializer<DataStream, ossia::domain_base<T>>
{
  using domain_t = ossia::domain_base<T>;
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
struct TSerializer<DataStream, ossia::domain_base<std::string>>
{
  using domain_t = ossia::domain_base<std::string>;
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
struct TSerializer<DataStream, ossia::domain_base<ossia::impulse>>
{
  using domain_t = ossia::domain_base<ossia::impulse>;
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
DataStreamReader::read(const ossia::domain& n)
{
  readFrom((const ossia::domain_base_variant&)n);
}


template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(ossia::domain& n)
{
  writeTo((ossia::domain_base_variant&)n);
}

/// JSON ///
template <>
ossia::impulse fromJsonValue<ossia::impulse>(const QJsonValueRef& obj)
{
  return {};
}

template <typename T, std::size_t N>
struct TSerializer<JSONValue, std::array<T, N>>
{
  static void
  readFrom(JSONValue::Serializer& s, const std::array<T, N>& vec)
  {
    QJsonArray arr;
    for (std::size_t i = 0; i < N; i++)
      arr.push_back(toJsonValue(vec[i]));
    s.val = std::move(arr);
  }

  static void
  writeTo(JSONValue::Deserializer& s, std::array<T, N>& vec)
  {
    auto arr = s.val.toArray();
    const std::size_t M = std::min((int)N, arr.size());
    for (std::size_t i = 0; i < M; i++)
      vec[i] = fromJsonValue<T>(arr[i]);
  }
};

template <std::size_t N>
struct TSerializer<JSONValue, std::array<float, N>>
{
  static void
  readFrom(JSONValue::Serializer& s, const std::array<float, N>& vec)
  {
    QJsonArray arr;
    for (std::size_t i = 0; i < N; i++)
      arr.push_back(vec[i]);
    s.val = std::move(arr);
  }

  static void
  writeTo(JSONValue::Deserializer& s,  std::array<float, N>& vec)
  {
    auto arr = s.val.toArray();
    const std::size_t M = std::min((int)N, arr.size());
    for (std::size_t i = 0; i < M; i++)
      vec[i] = arr[i].toDouble();
  }
};

template<typename T>
struct TSerializer<JSONValue, optional<T>>
{
  static void readFrom(JSONValue::Serializer& s, const optional<T>& obj)
  {
    if (obj)
    {
      s.val = toJsonValue(*obj);
    }
    else
    {
      s.val = QJsonValue{};
    }
  }

  static void writeTo(JSONValue::Deserializer& s, optional<T>& obj)
  {
    if (s.val.toString() == s.strings.none)
    {
      obj = ossia::none;
    }
    else
    {
      obj = fromJsonValue<T>(s.val);
    }
  }
};



QJsonValue toJsonValue(const optional<float>& f)
{
  if(f)
    return *f;
  else
    return QJsonValue{};
}

template <typename T>
QJsonArray toJsonArray(const boost::container::flat_set<T>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    arr.push_back(toJsonValue(v));
  return arr;
}

QJsonArray toJsonArray(const boost::container::flat_set<float>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    arr.push_back(v);
  return arr;
}

template<std::size_t N>
QJsonArray toJsonArray(const std::array<optional<float>, N>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    if(v) arr.push_back(*v);
    else arr.push_back(QJsonValue{});
  return arr;
}

QJsonArray toJsonArray(const std::vector<ossia::value>& array)
{
  QJsonArray arr;
  for (auto& v : array)
    arr.push_back(toJsonValue(v));
  return arr;
}

QJsonArray toJsonArray(
    const std::vector<boost::container::flat_set<ossia::value>>& array)
{
  QJsonArray arr;
  for (auto& v : array)
  {
    QJsonArray sub;
    for(auto& val : v)
      sub.push_back(toJsonValue(val));
    arr.push_back(std::move(sub));
  }
  return arr;
}

template<std::size_t N>
QJsonArray toJsonArray(
    const std::array<boost::container::flat_set<float>, N>& array)
{
  QJsonArray arr;
  for (auto& v : array)
  {
    QJsonArray sub;
    for(float val : v)
      sub.push_back(val);
    arr.push_back(std::move(sub));
  }
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
JSONObjectReader::read(const ossia::value& n);

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::write(ossia::value& n);

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::read(const ossia::value& n);
template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::write(ossia::value& n);

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
struct TSerializer<JSONObject, ossia::domain_base<T>>
{
  using domain_t = ossia::domain_base<T>;
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
struct TSerializer<JSONObject, ossia::domain_base<std::string>>
{
  using domain_t = ossia::domain_base<std::string>;
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
struct TSerializer<JSONObject, ossia::domain_base<ossia::impulse>>
{
  using domain_t = ossia::domain_base<ossia::impulse>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain)
  {
  }
};


template <>
struct TSerializer<JSONObject, ossia::vector_domain>
{
  using domain_t = ossia::vector_domain;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
    s.obj[s.strings.Min] = toJsonArray(domain.min);
    s.obj[s.strings.Max] = toJsonArray(domain.max);
    s.obj[s.strings.Values] = toJsonArray(domain.values);
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain)
  {
    // OPTIMIZEME there should be something in boost
    // to get multiple iterators from multiple keys in one pass...
    auto it_min = s.obj.constFind(s.strings.Min);
    auto it_max = s.obj.constFind(s.strings.Max);
    auto it_values = s.obj.constFind(s.strings.Values);
    if (it_min != s.obj.constEnd())
    {
      domain.min = fromJsonValue<std::vector<ossia::value>>(*it_min);
    }
    if (it_max != s.obj.constEnd())
    {
      domain.max = fromJsonValue<std::vector<ossia::value>>(*it_max);
    }
    if (it_values != s.obj.constEnd())
    {
      const auto arr = it_values->toArray();
      domain.values.resize(arr.size());
      int i = 0;
      for (const auto& v : arr)
      {
        if(v.isArray())
        {
          for(const auto& u : v.toArray())
            domain.values[i].insert(fromJsonValue<ossia::value>(u));
        }
        i++;
      }
    }
  }
};

template <typename T>
struct TSerializer<DataStream, boost::container::flat_set<T>>
{
  using type = boost::container::flat_set<T>;
  static void readFrom(DataStream::Serializer& s, const type& obj)
  {
    s.stream() << (int32_t)obj.size();
    for(const auto& e : obj)
      s.stream() << e;
  }

  static void writeTo(DataStream::Deserializer& s, type& obj)
  {
    int32_t n;
    s.stream() >> n;
    for(; n --> 0;)
    {
      T val;
      s.stream() >> val;
      obj.insert(std::move(val));
    }
  }
};

template <>
struct TSerializer<DataStream, ossia::vector_domain>
{
  using domain_t = ossia::vector_domain;
  static void readFrom(DataStream::Serializer& s, const domain_t& domain)
  {
    s.stream() << domain.min << domain.max << domain.values;
  }

  static void writeTo(DataStream::Deserializer& s, domain_t& domain)
  {
    s.stream() >> domain.min >> domain.max >> domain.values;
  }
};


template <std::size_t N>
struct TSerializer<JSONObject, ossia::vecf_domain<N>>
{
  using domain_t = ossia::vecf_domain<N>;
  static void readFrom(JSONObject::Serializer& s, const domain_t& domain)
  {
    s.obj[s.strings.Min] = toJsonArray(domain.min);
    s.obj[s.strings.Max] = toJsonArray(domain.max);
    s.obj[s.strings.Values] = toJsonArray(domain.values);
  }

  static void writeTo(JSONObject::Deserializer& s, domain_t& domain)
  {
    // OPTIMIZEME there should be something in boost
    // to get multiple iterators from multiple keys in one pass...
    auto it_min = s.obj.constFind(s.strings.Min);
    auto it_max = s.obj.constFind(s.strings.Max);
    auto it_values = s.obj.constFind(s.strings.Values);
    if (it_min != s.obj.constEnd())
    {
      domain.min = fromJsonValue<std::array<optional<float>, N>>(*it_min);
    }
    if (it_max != s.obj.constEnd())
    {
      domain.max = fromJsonValue<std::array<optional<float>, N>>(*it_max);
    }
    if (it_values != s.obj.constEnd())
    {
      const auto arr = it_values->toArray();
      std::size_t i = 0;
      for (const auto& v : arr)
      {
        if(i < N)
        {
          if(v.isArray())
          {
            for(const auto& u : v.toArray())
              domain.values[i].insert(fromJsonValue<float>(u));
          }
          i++;
        }
      }
    }
  }
};

template <std::size_t N>
struct TSerializer<DataStream, ossia::vecf_domain<N>>
{
  using domain_t = ossia::vecf_domain<N>;
  static void readFrom(DataStream::Serializer& s, const domain_t& domain)
  {
    s.stream() << domain.min << domain.max << domain.values;
  }

  static void writeTo(DataStream::Deserializer& s, domain_t& domain)
  {
    s.stream() >> domain.min >> domain.max >> domain.values;
  }
};



template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const ossia::domain& n)
{
  readFrom((const ossia::domain_base_variant&)n);
}


template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::write(ossia::domain& n)
{
  writeTo((ossia::domain_base_variant&)n);
}


template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const ossia::value& n)
{
  readFrom((const ossia::value_variant_type&)n.v);
}


template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::write(ossia::value& n)
{
  writeTo((ossia::value_variant_type&)n.v);
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::read(const ossia::value& n)
{
  val = marshall<JSONObject>(n);
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::write(ossia::value& n)
{
  n = unmarshall<ossia::value>(val.toObject());
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::read(const ossia::impulse& n)
{
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::write(ossia::impulse& n)
{
  val = QJsonValue{};
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::read(const std::array<float, 2>& n)
{
  val = toJsonValue(n);
}
template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::read(const std::array<float, 3>& n)
{
  val = toJsonValue(n);
}
template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::read(const std::array<float, 4>& n)
{
  val = toJsonValue(n);
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::write(std::array<float, 2>& n)
{
  fromJsonValue(val, n);
}
template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::write(std::array<float, 3>& n)
{
  fromJsonValue(val, n);
}
template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::write(std::array<float, 4>& n)
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
DataStreamWriter::write(ossia::net::instance_bounds& n)
{
  m_stream >> n.min_instances >> n.max_instances;
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueReader::read(const ossia::net::instance_bounds& b)
{
  QJsonObject obj;
  obj[strings.Min] = b.min_instances;
  obj[strings.Max] = b.max_instances;
  val = std::move(obj);
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONValueWriter::write(ossia::net::instance_bounds& n)
{
  const auto& obj = val.toObject();
  n.min_instances = obj[strings.Min].toInt();
  n.max_instances = obj[strings.Max].toInt();
}
