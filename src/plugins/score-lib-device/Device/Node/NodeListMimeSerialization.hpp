#pragma once
#include <Device/Node/DeviceNode.hpp>

#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MimeVisitor.hpp>

namespace score::mime
{
inline constexpr const char* nodelist() noexcept
{
  return "application/x-score-nodelist";
}
}
template <>
struct MimeReader<Device::NodeList> : public MimeDataReader
{
  using MimeDataReader::MimeDataReader;
  void serialize(const Device::NodeList& lst) const
  {
    JSONReader r;

    r.stream.StartArray();
    for (const auto& elt : lst)
    {
      r.stream.StartObject();
      r.obj["Node"] = *elt;
      r.obj["Address"] = Device::address(*elt).address;
      r.stream.EndObject();
    }
    r.stream.EndArray();

    m_mime.setData(score::mime::nodelist(), r.toByteArray());
  }
};

template <>
struct MimeWriter<Device::FreeNodeList> : public MimeDataWriter
{
  using MimeDataWriter::MimeDataWriter;
  auto deserialize()
  {
    Device::FreeNodeList ml;
    auto json = readJson(m_mime.data(score::mime::nodelist()));
    const auto& arr = json.GetArray();

    auto& strings = score::StringConstant();
    for (const rapidjson::Value& elt : arr)
    {
      JSONObject::Deserializer des{elt};
      Device::FreeNode n;
      n.first <<= des.obj[strings.Address];
      n.second <<= des.obj["Node"];
      ml.push_back(n);
    }

    return ml;
  }
};
