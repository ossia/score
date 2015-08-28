#include "ConstraintViewModelSerialization.hpp"
#include "ConstraintViewModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const ConstraintViewModel& cvm)
{
    // Add the constraint id since we need it for construction
    m_stream << cvm.model().id();

    // We happily do not require a way to save the derived type, since it is known
    // at compile time and calls this function.
    readFrom(static_cast<const IdentifiedObject<ConstraintViewModel>&>(cvm));

    // Save the ConstraintViewModelData
    m_stream << cvm.shownRack();
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ConstraintViewModel& cvm)
{
    Id<RackModel> id;
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
void Visitor<Reader<JSONObject>>::readFrom(const ConstraintViewModel& cvm)
{
    m_obj["ConstraintId"] = toJsonValue(cvm.model().id());

    readFrom(static_cast<const IdentifiedObject<ConstraintViewModel>&>(cvm));

    m_obj["ShownRack"] = toJsonValue(cvm.shownRack());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(ConstraintViewModel& cvm)
{
    auto id = fromJsonValue<Id<RackModel>>(m_obj["ShownRack"]);

    if(id.val())
    {
        cvm.showRack(id);
    }
    else
    {
        cvm.hideRack();
    }
}

