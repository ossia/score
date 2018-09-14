#pragma once
#include <State/Message.hpp>

#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <QJsonDocument>

namespace score
{
namespace mime
{
inline constexpr const char* messagelist()
{
  return "application/x-score-messagelist";
}
}
}

template <>
struct Visitor<Reader<Mime<State::MessageList>>> : public MimeDataReader
{
  using MimeDataReader::MimeDataReader;
  void serialize(const State::MessageList& lst) const
  {
    m_mime.setData(
        score::mime::messagelist(),
        QJsonDocument(toJsonArray(lst)).toJson(QJsonDocument::Indented));
  }
};

template <>
struct Visitor<Writer<Mime<State::MessageList>>> : public MimeDataWriter
{
  using MimeDataWriter::MimeDataWriter;
  auto deserialize()
  {
    State::MessageList ml;
    fromJsonArray(
        QJsonDocument::fromJson(m_mime.data(score::mime::messagelist()))
            .array(),
        ml);
    return ml;
  }
};

template <>
struct TSerializer<JSONObject, State::MessageList>
{
  static void
  readFrom(JSONObject::Serializer& s, const State::MessageList& obj)
  {
    s.obj[s.strings.Data] = toJsonArray(obj);
  }

  static void writeTo(JSONObject::Deserializer& s, State::MessageList& obj)
  {
    State::MessageList t;
    fromJsonArray(s.obj[s.strings.Data].toArray(), t);
    obj = t;
  }
};
