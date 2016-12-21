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
void Visitor<Reader<DataStream>>::readFrom_impl(
    const Scenario::ScenarioDocumentModel& obj)
{
  readFrom(
      static_cast<const IdentifiedObject<iscore::
                                             DocumentDelegateModel>&>(
          obj));
  readFrom(*obj.m_baseScenario);

  insertDelimiter();
}

template <>
void Visitor<Writer<DataStream>>::writeTo(Scenario::ScenarioDocumentModel& obj)
{
  obj.m_baseScenario = new Scenario::BaseScenario{*this, &obj};

  checkDelimiter();
}

template <>
void Visitor<Reader<JSONObject>>::readFrom_impl(
    const Scenario::ScenarioDocumentModel& obj)
{
  readFrom(
      static_cast<const IdentifiedObject<iscore::
                                             DocumentDelegateModel>&>(
          obj));
  m_obj["BaseScenario"] = toJsonObject(*obj.m_baseScenario);
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(Scenario::ScenarioDocumentModel& obj)
{
  writeTo(
      static_cast<IdentifiedObject<iscore::DocumentDelegateModel>&>(
          obj));
  obj.m_baseScenario = new Scenario::BaseScenario{
      Deserializer<JSONObject>{m_obj["BaseScenario"].toObject()}, &obj};
}

void Scenario::ScenarioDocumentModel::serialize(
    const VisitorVariant& vis) const
{
  serialize_dyn(vis, *this);
}
