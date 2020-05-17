#include "Preset.hpp"
#include <Process/ProcessList.hpp>
using JsonWriter = rapidjson::Writer<rapidjson::StringBuffer>;

namespace Process
{

std::shared_ptr<Preset> Preset::fromJson(const ProcessFactoryList& procs, const QByteArray& obj) noexcept
{
  rapidjson::Document doc;
  Process::Preset p;
  doc.Parse(obj.data(), obj.size());

  JSONWriter wr{doc};
  const auto& k = wr.obj["Key"];
  p.key.key <<= k["Uuid"];
  p.key.effect = k["Effect"].toString();

  if(!procs.get(p.key.key))
    return {};

  rapidjson::StringBuffer buf;
  JsonWriter writer(buf);
  wr.obj["Preset"].obj.Accept(writer);
  p.data = QByteArray(buf.GetString(), buf.GetSize());
  p.name = wr.obj["Name"].toString();

  return std::make_shared<Process::Preset>(std::move(p));
}

QByteArray Preset::toJson() const noexcept
{
  JSONReader ser;
  ser.readFrom(*this);
  return ser.toByteArray();
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
JSONReader::read(const Process::Preset& p)
{
  stream.StartObject();
  stream.Key("Key");
  stream.StartObject();
  stream.Key("Uuid");
  readFrom(p.key.key);
  stream.Key("Effect");
  readFrom(p.key.effect);
  stream.EndObject();

  obj["Name"] = p.name;

  {
    // TODO not very optimal...
    rapidjson::Document doc;
    doc.Parse(p.data.data(), p.data.size());

    stream.Key("Preset");
    doc.Accept(stream);
  }
  stream.EndObject();
}

// We only load the members of the process here.
template <>
SCORE_LIB_PROCESS_EXPORT void
JSONWriter::write(Process::Preset& p)
{
  const auto& k = obj["Key"];
  p.key.key <<= k["Uuid"];
  p.key.effect = k["Effect"].toString();

  {
    // TODO not very optimal...
    rapidjson::StringBuffer buf;
    JsonWriter writer(buf);
    obj["Preset"].obj.Accept(writer);
    p.data = QByteArray(buf.GetString(), buf.GetSize());
  }
  p.name = obj["Name"].toString();
}
