#include "Preset.hpp"
#include <Process/ProcessList.hpp>

namespace Process
{

std::optional<Preset> Preset::fromJson(const ProcessFactoryList& procs, const QJsonObject& obj) noexcept
{
  auto k = obj["Key"].toObject();
  auto uuid_k = fromJsonValue<score::uuid_t>(k["Uuid"]);
  auto sub_k = k["Effect"].toString();

  auto it = procs.find(uuid_k);
  if(it == procs.end())
    return std::nullopt;

  Process::Preset p;
  p.key = {uuid_k, sub_k};
  p.data = obj["Preset"].toObject();
  p.name = obj["Name"].toString();

  return p;
}

QJsonObject Preset::toJson() const noexcept
{
  QJsonObject presetObj;
  QJsonObject keyObj;
  keyObj["Uuid"] = toJsonValue(key.key);
  keyObj["Effect"] = key.effect;

  presetObj["Name"] = name;
  presetObj["Key"] = keyObj;
  presetObj["Preset"] = data;
  return presetObj;
}

}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::Preset& p)
{
  m_stream << p.key.key << p.key.effect << p.data << p.name;
}

// We only load the members of the process here.
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::Preset& p)
{
  m_stream >> p.key.key >> p.key.effect >> p.data >> p.name;
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read(const Process::Preset& p)
{
  obj = p.toJson();
}

// We only load the members of the process here.
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write(Process::Preset& p)
{
  auto k = obj["Key"].toObject();
  auto uuid_k = fromJsonValue<score::uuid_t>(k["Uuid"]);
  auto sub_k = k["Effect"].toString();

  p.key = {uuid_k, sub_k};
  p.data = obj["Preset"].toObject();
  p.name = obj["Name"].toString();
}
