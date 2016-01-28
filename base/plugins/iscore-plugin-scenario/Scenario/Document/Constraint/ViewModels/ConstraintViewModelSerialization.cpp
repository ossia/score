
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
template <typename T> class Reader;
template <typename T> class Writer;
template <typename model> class IdentifiedObject;

template<>
void Visitor<Reader<DataStream>>::readFrom(const Scenario::ConstraintViewModel& cvm)
{
    // Add the constraint id since we need it for construction
    m_stream << cvm.model().id();

    // We happily do not require a way to save the derived type, since it is known
    // at compile time and calls this function.
    readFrom(static_cast<const IdentifiedObject<Scenario::ConstraintViewModel>&>(cvm));

    // Save the ConstraintViewModelData
    m_stream << cvm.shownRack();
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Scenario::ConstraintViewModel& cvm)
{
    Id<Scenario::RackModel> id;
    m_stream >> id;

    if(id.val())
    {
        cvm.showRack(id);
    }
    else
    {
        cvm.hideRack();
    }

    checkDelimiter();
}


template<>
void Visitor<Reader<JSONObject>>::readFrom(const Scenario::ConstraintViewModel& cvm)
{
    m_obj["ConstraintId"] = toJsonValue(cvm.model().id());

    readFrom(static_cast<const IdentifiedObject<Scenario::ConstraintViewModel>&>(cvm));

    m_obj["ShownRack"] = toJsonValue(cvm.shownRack());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Scenario::ConstraintViewModel& cvm)
{
    auto id = fromJsonValue<Id<Scenario::RackModel>>(m_obj["ShownRack"]);

    if(id.val())
    {
        cvm.showRack(id);
    }
    else
    {
        cvm.hideRack();
    }
}

