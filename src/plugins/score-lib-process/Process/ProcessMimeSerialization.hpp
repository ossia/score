#pragma once
#include <Process/ProcessFactory.hpp>

#include <score/plugins/SerializableInterface.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/MimeVisitor.hpp>

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
inline constexpr auto processcontrol()
{
  return "application/x-score-processcontrol";
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
struct MimeReader<Process::ProcessData> : public MimeDataReader
{
  using MimeDataReader::MimeDataReader;
  void serialize(const Process::ProcessData& lst) const
  {
    m_mime.setData(score::mime::processdata(), DataStreamReader::marshall(lst));
  }
};

template <>
struct MimeWriter<Process::ProcessData> : public MimeDataWriter
{
  using MimeDataWriter::MimeDataWriter;
  auto deserialize()
  {
    return DataStreamWriter::unmarshall<Process::ProcessData>(
        m_mime.data(score::mime::processdata()));
  }
};
