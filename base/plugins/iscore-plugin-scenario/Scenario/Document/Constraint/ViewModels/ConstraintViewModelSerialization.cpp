
#include <QJsonValue>
#include <algorithm>

#include "ConstraintViewModel.hpp"
#include "ConstraintViewModelSerialization.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

namespace Scenario
{
class RackModel;
}

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamReader::read(const Scenario::ConstraintViewModel& cvm)
{
  m_stream << cvm.shownRack();
  insertDelimiter();
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::writeTo(Scenario::ConstraintViewModel& cvm)
{
  OptionalId<Scenario::RackModel> id;
  m_stream >> id;

  if (id)
  {
    cvm.showRack(*id);
  }
  else
  {
    cvm.hideRack();
  }

  checkDelimiter();
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectReader::readFrom(const Scenario::ConstraintViewModel& cvm)
{
  readFrom(static_cast<const IdentifiedObject<Scenario::ConstraintViewModel>&>(
      cvm));

  obj["ShownRack"] = toJsonValue(cvm.shownRack());
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectWriter::writeTo(Scenario::ConstraintViewModel& cvm)
{
  auto id = fromJsonValue<OptionalId<Scenario::RackModel>>(obj["ShownRack"]);

  if (id)
  {
    cvm.showRack(*id);
  }
  else
  {
    cvm.hideRack();
  }
}
