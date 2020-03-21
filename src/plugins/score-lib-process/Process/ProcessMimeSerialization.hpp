#pragma once
#include <Process/ProcessFactory.hpp>

#include <score/plugins/SerializableInterface.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <QJsonDocument>
namespace score
{
namespace mime
{
inline constexpr auto processdata()
{
  return "application/x-score-processdata";
}
inline constexpr auto processpreset()
{
  return "application/x-score-processpreset";
}
inline constexpr auto layerdata()
{
  return "application/x-score-layerdata";
}
inline constexpr auto scenariodata()
{
  return "application/x-score-scenariodata";
}
}
}

namespace Process
{
struct ProcessData
{
  UuidKey<Process::ProcessModel> key;
  QString prettyName;
  QString customData;
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
    obj["PrettyName"] = lst.prettyName;
    obj["Data"] = lst.customData;
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
    auto obj = QJsonDocument::fromJson(m_mime.data(score::mime::processdata()))
                   .object();
    Process::ProcessData p;
    p.key = fromJsonValue<score::uuid_t>(obj["uuid"]);
    p.customData = obj["Data"].toString();
    p.prettyName = obj["PrettyName"].toString();
    return p;
  }
};
