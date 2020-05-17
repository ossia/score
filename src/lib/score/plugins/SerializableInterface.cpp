// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SerializableInterface.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/plugins/UuidKey.hpp>
#include <boost/uuid/nil_generator.hpp>

namespace score
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

QByteArray toByteArray(uuid const& u)
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


void TSerializer<DataStream, score::uuid_t>::readFrom(DataStream::Serializer& s, const score::uuid_t& uid)
{
  SCORE_DEBUG_INSERT_DELIMITER2(s);
  s.stream().stream.writeRawData(
      (const char*)uid.data, sizeof(uid.data));
  SCORE_DEBUG_INSERT_DELIMITER2(s);
}

void TSerializer<DataStream, score::uuid_t>::writeTo(DataStream::Deserializer& s, score::uuid_t& uid)
{
  SCORE_DEBUG_CHECK_DELIMITER2(s);
  s.stream().stream.readRawData(
      (char*)uid.data, sizeof(uid.data));
  SCORE_DEBUG_CHECK_DELIMITER2(s);
}

void TSerializer<JSONObject, score::uuid_t>::readFrom(JSONObject::Serializer& s, const score::uuid_t& uid)
{
  JSONReader::assigner{s} = score::uuids::toByteArray(uid);
}

void TSerializer<JSONObject, score::uuid_t>::writeTo(JSONObject::Deserializer& s, score::uuid_t& uid)
{
  QByteArray str = JsonValue{s.base}.toByteArray();
  uid = score::uuids::string_generator::compute(str.begin(), str.end());
}

