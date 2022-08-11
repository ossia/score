#pragma once
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
struct MimeReader<State::MessageList> : public MimeDataReader
{
  using MimeDataReader::MimeDataReader;
  void serialize(const State::MessageList& lst) const
  {
    m_mime.setData(score::mime::messagelist(), toJson(lst));
  }
};

template <>
struct MimeWriter<State::MessageList> : public MimeDataWriter
{
  using MimeDataWriter::MimeDataWriter;
  State::MessageList deserialize()
  {
    return fromJson<State::MessageList>(m_mime.data(score::mime::messagelist()));
  }
};
