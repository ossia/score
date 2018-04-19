// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SerializableInterface.hpp"

#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

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
SCORE_LIB_BASE_EXPORT void DataStreamReader::read(const score::uuid_t& obj)
{
  m_stream << score::uuids::toByteArray(obj);
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(score::uuid_t& obj)
{
  QByteArray s;
  m_stream >> s;
  obj = score::uuids::string_generator::compute(s.begin(), s.end());
}

template <>
SCORE_LIB_BASE_EXPORT void JSONValueReader::read(const score::uuid_t& obj)
{
  val = QString(score::uuids::toByteArray(obj));
}

template <>
SCORE_LIB_BASE_EXPORT void JSONValueWriter::write(score::uuid_t& obj)
{
  auto str = val.toString();
  obj = score::uuids::string_generator::compute(str.begin(), str.end());
}
