#include "Preset.hpp"

#include <Process/ProcessList.hpp>

#include <score/serialization/JSONVisitor.hpp>

namespace Process
{

std::shared_ptr<Preset>
Preset::fromJson(const ProcessFactoryList& procs, const QByteArray& obj) noexcept
{
  rapidjson::Document doc;
  Process::Preset p;
  doc.Parse(obj.data(), obj.size());
  if(doc.HasParseError())
    return {};

  if(!doc.IsObject())
    return {};

  if(!doc.GetObject().HasMember("Key")
     || !(doc.GetObject().HasMember("Preset") || doc.GetObject().HasMember("PresetData")))
    return {};

  JSONWriter wr{doc};
  const auto& k = wr.obj["Key"];
  p.key.key <<= k["Uuid"];
  p.key.effect = k["Effect"].toString();

  if(!procs.get(p.key.key))
    return {};

  if(auto raw = wr.obj.tryGet("PresetData"))
  {
    p.data = QByteArray::fromBase64(raw->toByteArray());
  }
  else
  {
    rapidjson::StringBuffer buf;
    JsonWriter writer(buf);
    wr.obj["Preset"].obj.Accept(writer);
    p.data = QByteArray(buf.GetString(), buf.GetSize());
  }
  p.name = wr.obj["Name"].toString();

  // Optional: old presets do not have a category
  if(doc.GetObject().HasMember("Category"))
    p.category = wr.obj["Category"].toString();
  if(doc.GetObject().HasMember("Description"))
    p.description = wr.obj["Description"].toString();

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
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::Preset& p)
{
  m_stream << p.key.key << p.key.effect << p.data << p.name << p.category
           << p.description;
}

// We only load the members of the process here.
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::Preset& p)
{
  m_stream >> p.key.key >> p.key.effect >> p.data >> p.name >> p.category
      >> p.description;
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::Preset& p)
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
  if(!p.category.isEmpty())
    obj["Category"] = p.category;
  if(!p.description.isEmpty())
    obj["Description"] = p.description;

  {
    // TODO not very optimal...
    rapidjson::Document doc;
    doc.Parse(p.data.data(), p.data.size());

    if(!doc.HasParseError())
    {
      stream.Key("Preset");
      doc.Accept(stream);
    }
    else
    {
      // A process whose data is not JSON would otherwise be saved as null
      stream.Key("PresetData");
      readFrom(QString::fromLatin1(p.data.toBase64()));
    }
  }
  stream.EndObject();
}

// We only load the members of the process here.
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::Preset& p)
{
  const auto& k = obj["Key"];
  p.key.key <<= k["Uuid"];
  p.key.effect = k["Effect"].toString();

  if(auto raw = obj.tryGet("PresetData"))
  {
    p.data = QByteArray::fromBase64(raw->toByteArray());
  }
  else
  {
    // TODO not very optimal...
    rapidjson::StringBuffer buf;
    JsonWriter writer(buf);
    obj["Preset"].obj.Accept(writer);
    p.data = QByteArray(buf.GetString(), buf.GetSize());
  }
  p.name = obj["Name"].toString();
  if(auto cat = obj.tryGet("Category"))
    p.category = cat->toString();
  if(auto desc = obj.tryGet("Description"))
    p.description = desc->toString();
}
