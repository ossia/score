// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>
#include <algorithm>
#include <score/serialization/VisitorCommon.hpp>

#include "ScenarioDocumentModel.hpp"
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

namespace score
{
class DocumentDelegateModel;
} // namespace score
struct VisitorVariant;
template <typename T>
class Reader;
template <typename T>
class Writer;
template <typename model>
class IdentifiedObject;


template <>
void DataStreamReader::read(
    const Scenario::ScenarioDocumentModel& obj)
{
  readFrom(*obj.m_baseScenario);

  m_stream << (int32_t)obj.cables.size();
  for (const auto& p : obj.cables)
  {
    readFrom(p);
  }

  insertDelimiter();
}


template <>
void DataStreamWriter::write(Scenario::ScenarioDocumentModel& obj)
{
  obj.m_baseScenario = new Scenario::BaseScenario{*this, &obj};

  int32_t cable_count;
  m_stream >> cable_count;
  for (; cable_count-- > 0;)
  {
    obj.cables.add(new Process::Cable{*this, &obj});
  }

  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Scenario::ScenarioDocumentModel& doc)
{
  obj["BaseScenario"] = toJsonObject(*doc.m_baseScenario);
  obj["Cables"] = toJsonArray(doc.cables);
}


template <>
void JSONObjectWriter::write(Scenario::ScenarioDocumentModel& doc)
{
  doc.m_baseScenario = new Scenario::BaseScenario(
        JSONObject::Deserializer{obj["BaseScenario"].toObject()}, &doc);

  doc.m_savedCables = obj["Cables"].toArray();

}

void Scenario::ScenarioDocumentModel::serialize(
    const VisitorVariant& vis) const
{

  serialize_dyn(vis, *this);
}
