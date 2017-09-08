#pragma once
#include <QJsonDocument>
#include <State/Message.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MimeVisitor.hpp>

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
