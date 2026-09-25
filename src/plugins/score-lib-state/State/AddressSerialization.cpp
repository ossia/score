// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <State/Expression.hpp>

#include <ossia/detail/algorithms.hpp>
#include "Address.hpp"

#include <State/Unit.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QIODevice>

#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

SCORE_SERALIZE_DATASTREAM_DEFINE(State::Address)
SCORE_SERALIZE_DATASTREAM_DEFINE(State::DestinationQualifiers)
SCORE_SERALIZE_DATASTREAM_DEFINE(State::AddressAccessor)
/// Address ///
namespace State
{
void readAnchor(DataStreamReader& r, const std::shared_ptr<const Anchor>& a)
{
  r.m_stream << bool(a);
  if(a)
    r.m_stream << a->target << a->member;
}

void writeAnchor(DataStreamWriter& w, std::shared_ptr<const Anchor>& a)
{
  a.reset();
  auto& stream = w.m_stream.stream;
  bool anchored{};
  stream >> anchored;
  if(!anchored || stream.status() != QDataStream::Ok)
    return;

  // Bound the path's element count by what is left in the stream
  if(const auto dev = stream.device())
  {
    const auto head = dev->peek(sizeof(int32_t));
    QDataStream peek{head};
    peek.setByteOrder(stream.byteOrder());
    peek.setVersion(stream.version());
    int32_t n{};
    peek >> n;
    if(peek.status() != QDataStream::Ok || n < 0 || n > dev->bytesAvailable())
    {
      stream.setStatus(QDataStream::ReadCorruptData);
      return;
    }
  }

  Anchor an;
  w.m_stream >> an.target >> an.member;
  a = std::make_shared<const Anchor>(std::move(an));
}
}

namespace
{
// A plain address is its string; an anchored one carries the object it stands for.
void readAnchored(JSONReader& r, const QString& str, const State::Anchor& a)
{
  r.stream.StartObject();
  r.obj[r.strings.Address] = str;
  r.obj["Target"] = a.target;
  if(!a.member.isEmpty())
    r.obj["Member"] = a.member;
  r.stream.EndObject();
}

std::shared_ptr<const State::Anchor> anchorFromJson(JSONWriter& w)
{
  auto t = w.obj.tryGet("Target");
  if(!t || !t->obj.IsArray() || t->obj.Empty())
    return {};
  State::Anchor an;
  an.target <<= *t;
  if(auto m = w.obj.tryGet("Member"); m && m->obj.IsString())
    an.member = m->toString();
  return std::make_shared<const State::Anchor>(std::move(an));
}
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const State::Address& a)
{
  m_stream << a.device << a.path;
  State::readAnchor(*this, a.anchor);
  insertDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONReader::read(const State::Address& a)
{
  if(a.anchor)
  {
    readAnchored(*this, a.toString(), *a.anchor);
  }
  else
  {
    const auto& str = a.toString().toUtf8();
    stream.String(str.data(), str.size());
  }
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(State::Address& a)
{
  m_stream >> a.device >> a.path;
  State::writeAnchor(*this, a.anchor);
  checkDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONWriter::write(State::Address& a)
{
  if(base.IsString())
  {
    if(auto addr = State::parseAddress(
           QString::fromUtf8(base.GetString(), base.GetStringLength())))
      a = *std::move(addr);
  }
  else if(base.IsObject())
  {
    if(auto str = obj.tryGet(strings.Address); str && str->obj.IsString())
      if(auto addr = State::parseAddress(str->toString()))
        a = *std::move(addr);
    a.anchor = anchorFromJson(*this);
  }
}

/// AddressQualifiers ///
template <>
SCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const ossia::destination_qualifiers& a)
{
  m_stream << a.accessors << a.unit;
}

template <>
SCORE_LIB_STATE_EXPORT void JSONReader::read(const ossia::destination_qualifiers& a)
{
  obj[strings.Accessors] = a.accessors;
  obj[strings.Unit] = State::prettyUnitText(a.unit);
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(ossia::destination_qualifiers& a)
{
  m_stream >> a.accessors >> a.unit;
}

template <>
SCORE_LIB_STATE_EXPORT void JSONWriter::write(ossia::destination_qualifiers& a)
{
  a.accessors <<= obj[strings.Accessors];
  a.unit = ossia::parse_pretty_unit(obj[strings.Unit].toStdString());
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const State::DestinationQualifiers& a)
{
  m_stream << a.get();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONReader::read(const State::DestinationQualifiers& a)
{
  read(a.get());
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(State::DestinationQualifiers& a)
{
  m_stream >> a.get();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONWriter::write(State::DestinationQualifiers& a)
{
  write(a.get());
}

/// AddressAccessor ///
template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const State::AddressAccessor& rel)
{
  m_stream << rel.address << rel.qualifiers;

  insertDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONReader::read(const State::AddressAccessor& rel)
{
  if(rel.address.anchor)
  {
    readAnchored(*this, rel.toString(), *rel.address.anchor);
  }
  else
  {
    const auto& str = rel.toString().toUtf8();
    stream.String(str.data(), str.size());
  }
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(State::AddressAccessor& rel)
{
  m_stream >> rel.address >> rel.qualifiers;

  checkDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONWriter::write(State::AddressAccessor& rel)
{
  if(base.IsString())
  {
    if(auto addr = State::parseAddressAccessor(
           QString::fromUtf8(base.GetString(), base.GetStringLength())))
      rel = *std::move(addr);
  }
  else if(base.IsObject())
  {
    if(auto str = obj.tryGet(strings.Address); str && str->obj.IsString())
      if(auto addr = State::parseAddressAccessor(str->toString()))
        rel = *std::move(addr);
    rel.address.anchor = anchorFromJson(*this);
  }
}

/// AddressAccessorHead ///
template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const State::AddressAccessorHead& rel)
{
  m_stream << rel.name << rel.qualifiers;

  insertDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONReader::read(const State::AddressAccessorHead& rel)
{
  obj[strings.Name] = rel.name;
  readFrom(rel.qualifiers);
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(State::AddressAccessorHead& rel)
{
  m_stream >> rel.name >> rel.qualifiers;

  checkDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONWriter::write(State::AddressAccessorHead& rel)
{
  rel.name = obj[strings.Name].toString();
  writeTo(rel.qualifiers);
}

namespace State
{
void saveAnchors(JSONReader& r, const char* key, const Expression& e)
{
  const auto list = anchors(e);
  if(ossia::none_of(list, [](auto& a) { return bool(a); }))
    return;
  r.stream.Key(key);
  r.stream.StartArray();
  for(auto& a : list)
  {
    if(!a)
    {
      r.stream.Null();
      continue;
    }
    r.stream.StartObject();
    r.obj["Target"] = a->target;
    if(!a->member.isEmpty())
      r.obj["Member"] = a->member;
    r.stream.EndObject();
  }
  r.stream.EndArray();
}

void loadAnchors(JSONWriter& w, const char* key, Expression& e)
{
  auto it = w.base.FindMember(key);
  if(it == w.base.MemberEnd() || !it->value.IsArray())
    return;
  std::vector<std::shared_ptr<const Anchor>> list;
  for(auto& v : it->value.GetArray())
  {
    if(!v.IsObject())
    {
      list.push_back(nullptr);
      continue;
    }
    JSONWriter sub{v};
    list.push_back(anchorFromJson(sub));
  }
  setAnchors(e, list);
}
}
