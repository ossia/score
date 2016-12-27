#include <Scenario/Document/BaseScenario/BaseScenario.hpp>

#include <QJsonObject>
#include <QJsonValue>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>
#include <algorithm>
#include <iscore/serialization/VisitorCommon.hpp>

#include "ScenarioDocumentModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

namespace iscore
{
class DocumentDelegateModel;
} // namespace iscore
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

  insertDelimiter();
}


template <>
void DataStreamWriter::writeTo(Scenario::ScenarioDocumentModel& obj)
{
  obj.m_baseScenario = new Scenario::BaseScenario{*this, &obj};

  checkDelimiter();
}


template <>
void JSONObjectReader::readFromConcrete(
    const Scenario::ScenarioDocumentModel& doc)
{
  readFrom(
      static_cast<const IdentifiedObject<iscore::
                                             DocumentDelegateModel>&>(
          doc));
  obj["BaseScenario"] = toJsonObject(*doc.m_baseScenario);
}


template <>
void JSONObjectWriter::writeTo(Scenario::ScenarioDocumentModel& doc)
{
  writeTo(
      static_cast<IdentifiedObject<iscore::DocumentDelegateModel>&>(
          doc));
  doc.m_baseScenario = new Scenario::BaseScenario{
      JSONObject::Deserializer{obj["BaseScenario"].toObject()}, &doc};
}

void Scenario::ScenarioDocumentModel::serialize(
    const VisitorVariant& vis) const
{
  serialize_dyn(vis, *this);
}
