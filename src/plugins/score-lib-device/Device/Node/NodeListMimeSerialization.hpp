#pragma once
#include <Device/Node/DeviceNode.hpp>

#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <QDebug>

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
    for(const auto& elt : lst)
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

    // A drag source can declare score::mime::nodelist() with an empty or
    // malformed payload (an interrupted drag, another app echoing the type
    // with no data). readJson then yields a document that is not an array,
    // and rapidjson's GetArray() asserts IsArray() — a SIGABRT that takes
    // down the whole application. Refuse it: an unusable payload is an
    // empty node list, not a crash.
    if(!json.IsArray())
    {
      qWarning() << "nodelist drop: payload is not a JSON array ("
                 << m_mime.data(score::mime::nodelist()).size()
                 << "bytes); nothing dropped";
      return ml;
    }
    const auto& arr = json.GetArray();

    auto& strings = score::StringConstant();
    for(const rapidjson::Value& elt : arr)
    {
      // The array's CONTENTS are the same hazard as the array itself, and the
      // IsArray() guard above does not cover them: rapidjson's operator[]
      // asserts IsObject() on the value and asserts again when the member is
      // absent, so a perfectly well-formed `[1,2,3]` or `[{}]` is another
      // SIGABRT reachable from any external drag source. Skip an entry we
      // cannot read, and say which one.
      if(!elt.IsObject())
      {
        qWarning() << "nodelist drop: ignoring entry" << int(&elt - arr.Begin())
                   << "- not a JSON object";
        continue;
      }
      if(elt.FindMember(strings.Address.c_str()) == elt.MemberEnd()
         || elt.FindMember("Node") == elt.MemberEnd())
      {
        qWarning() << "nodelist drop: ignoring entry" << int(&elt - arr.Begin())
                   << "- missing its Address or Node member";
        continue;
      }

      JSONObject::Deserializer des{elt};
      Device::FreeNode n;
      n.first <<= des.obj[strings.Address];
      n.second <<= des.obj["Node"];
      ml.push_back(n);
    }

    return ml;
  }
};
