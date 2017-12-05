#pragma once
#include <Process/ProcessFactory.hpp>
#include <QJsonDocument>
#include <score/plugins/customfactory/SerializableInterface.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MimeVisitor.hpp>
namespace score
{
namespace mime
{
inline constexpr auto processdata()
{
  return "application/x-score-processdata";
}
inline constexpr auto layerdata()
{
  return "application/x-score-layerdata";
}
}
}

namespace Process
{
struct ProcessData
{
  UuidKey<Process::ProcessModel> key;
};
}
template <>
struct Visitor<Reader<Mime<Process::ProcessData>>> : public MimeDataReader
{
  using MimeDataReader::MimeDataReader;
  void serialize(const Process::ProcessData& lst) const
  {
    QJsonObject obj;
    obj["Type"] = "Process";
    obj["uuid"] = toJsonValue(lst.key.impl());
    m_mime.setData(
        score::mime::processdata(),
        QJsonDocument(obj).toJson(QJsonDocument::Indented));
  }
};

template <>
struct Visitor<Writer<Mime<Process::ProcessData>>> : public MimeDataWriter
{
  using MimeDataWriter::MimeDataWriter;
  auto deserialize()
  {
    auto obj
        = QJsonDocument::fromJson(m_mime.data(score::mime::processdata()))
              .object();
    Process::ProcessData p;
    p.key = fromJsonValue<score::uuid_t>(obj["uuid"]);
    return p;
  }
};
