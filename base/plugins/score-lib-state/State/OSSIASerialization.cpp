// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <State/DomainSerializationImpl.hpp>
#include <State/ValueSerializationImpl.hpp>

template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const ossia::domain& n)
{
  readFrom((const ossia::domain_base_variant&)n);
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(ossia::domain& n)
{
  writeTo((ossia::domain_base_variant&)n);
}

template <>
SCORE_LIB_STATE_EXPORT void JSONObjectReader::read(const ossia::domain& n)
{
  readFrom((const ossia::domain_base_variant&)n);
}

template <>
SCORE_LIB_STATE_EXPORT void JSONObjectWriter::write(ossia::domain& n)
{
  writeTo((ossia::domain_base_variant&)n);
}

template <>
SCORE_LIB_STATE_EXPORT void JSONObjectReader::read(const ossia::value& n)
{
  readFrom((const ossia::value_variant_type&)n.v);
}

template <>
SCORE_LIB_STATE_EXPORT void JSONObjectWriter::write(ossia::value& n)
{
  // REMOVEME in score 2.0 temporary check for the old case.
  auto it = obj.constFind(strings.Type);
  if (it == obj.constEnd())
    writeTo((ossia::value_variant_type&)n.v);
  else
    n = State::convert::fromQJsonValue(obj[strings.Value], it->toString());
}

template <>
SCORE_LIB_STATE_EXPORT void JSONValueReader::read(const ossia::value& n)
{
  val = score::marshall<JSONObject>(n);
}

template <>
SCORE_LIB_STATE_EXPORT void JSONValueWriter::write(ossia::value& n)
{
  n = score::unmarshall<ossia::value>(val.toObject());
}

template <>
SCORE_LIB_STATE_EXPORT void JSONValueReader::read(const ossia::impulse& n)
{
}

template <>
SCORE_LIB_STATE_EXPORT void JSONValueWriter::write(ossia::impulse& n)
{
  val = QJsonValue{};
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONValueReader::read(const std::array<float, 2>& n)
{
  val = toJsonValue(n);
}
template <>
SCORE_LIB_STATE_EXPORT void
JSONValueReader::read(const std::array<float, 3>& n)
{
  val = toJsonValue(n);
}
template <>
SCORE_LIB_STATE_EXPORT void
JSONValueReader::read(const std::array<float, 4>& n)
{
  val = toJsonValue(n);
}

template <>
SCORE_LIB_STATE_EXPORT void JSONValueWriter::write(std::array<float, 2>& n)
{
  fromJsonValue(val, n);
}
template <>
SCORE_LIB_STATE_EXPORT void JSONValueWriter::write(std::array<float, 3>& n)
{
  fromJsonValue(val, n);
}
template <>
SCORE_LIB_STATE_EXPORT void JSONValueWriter::write(std::array<float, 4>& n)
{
  fromJsonValue(val, n);
}

/// Instance bounds ///

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const ossia::net::instance_bounds& n)
{
  m_stream << n.min_instances << n.max_instances;
}

template <>
SCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(ossia::net::instance_bounds& n)
{
  m_stream >> n.min_instances >> n.max_instances;
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONValueReader::read(const ossia::net::instance_bounds& b)
{
  QJsonObject obj;
  obj[strings.Min] = b.min_instances;
  obj[strings.Max] = b.max_instances;
  val = std::move(obj);
}

template <>
SCORE_LIB_STATE_EXPORT void
JSONValueWriter::write(ossia::net::instance_bounds& n)
{
  const auto& obj = val.toObject();
  n.min_instances = obj[strings.Min].toInt();
  n.max_instances = obj[strings.Max].toInt();
}
