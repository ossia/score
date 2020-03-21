#pragma once
#include <Device/Node/DeviceNode.hpp>

#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <QJsonDocument>

namespace score::mime
{
inline constexpr const char* nodelist() noexcept
{
  return "application/x-score-nodelist";
}
}
template <>
struct Visitor<Reader<Mime<Device::NodeList>>> : public MimeDataReader
{
  using MimeDataReader::MimeDataReader;
  void serialize(const Device::NodeList& lst) const
  {
    QJsonArray arr;

    for (const auto& elt : lst)
    {
      auto node_obj = toJsonObject(*elt);
      node_obj.insert("Address", toJsonObject(Device::address(*elt).address));
      arr.append(std::move(node_obj));
    }

    m_mime.setData(
        score::mime::nodelist(),
        QJsonDocument(std::move(arr)).toJson(QJsonDocument::Indented));
  }
};

template <>
struct Visitor<Writer<Mime<Device::FreeNodeList>>> : public MimeDataWriter
{
  using MimeDataWriter::MimeDataWriter;
  auto deserialize()
  {
    Device::FreeNodeList ml;
    auto arr = QJsonDocument::fromJson(m_mime.data(score::mime::nodelist()))
                   .array();

    auto& strings = score::StringConstant();
    for (const auto& elt : arr)
    {
      Device::FreeNode n;
      auto obj = elt.toObject();
      n.first = fromJsonObject<State::Address>(obj[strings.Address]);

      JSONObject::Deserializer des{obj};
      des.writeTo(n.second);
      ml.push_back(n);
    }

    return ml;
  }
};
