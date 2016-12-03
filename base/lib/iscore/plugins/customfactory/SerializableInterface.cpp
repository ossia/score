#include "SerializableInterface.hpp"
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
namespace iscore
{
namespace uuids
{
static Q_RELAXED_CONSTEXPR char to_char(size_t i)
{
  if (i <= 9)
  {
    return static_cast<char>('0' + i);
  }
  else
  {
    return static_cast<char>('a' + (i - 10));
  }
}

static QByteArray toByteArray(uuid const& u)
{
  QByteArray result;
  result.reserve(36);

  std::size_t i = 0;
  for (auto it_data = u.begin(); it_data != u.end(); ++it_data, ++i)
  {
    const size_t hi = ((*it_data) >> 4) & 0x0F;
    result += to_char(hi);

    const size_t lo = (*it_data) & 0x0F;
    result += to_char(lo);

    if (i == 3 || i == 5 || i == 7 || i == 9)
    {
      result += '-';
    }
  }
  return result;
}
}
}

template <>
ISCORE_LIB_BASE_EXPORT void
Visitor<Reader<DataStream>>::readFrom(const iscore::uuid_t& obj)
{
  m_stream << iscore::uuids::toByteArray(obj);
}

template <>
ISCORE_LIB_BASE_EXPORT void
Visitor<Writer<DataStream>>::writeTo(iscore::uuid_t& obj)
{
  QByteArray s;
  m_stream >> s;
  obj = iscore::uuids::string_generator::compute(s.begin(), s.end());
}

template <>
ISCORE_LIB_BASE_EXPORT void
Visitor<Reader<JSONValue>>::readFrom(const iscore::uuid_t& obj)
{
  val = QString(iscore::uuids::toByteArray(obj));
}

template <>
ISCORE_LIB_BASE_EXPORT void
Visitor<Writer<JSONValue>>::writeTo(iscore::uuid_t& obj)
{
  auto str = val.toString();
  obj = iscore::uuids::string_generator::compute(str.begin(), str.end());
}
