#include "Process/AbstractScenarioLayerModel.hpp"
#include "Document/Constraint/ViewModels/ConstraintViewModelSerialization.hpp"
#include "TemporalScenarioLayerModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const TemporalScenarioLayerModel& lm)
{
    auto constraints = constraintsViewModels(lm);

    m_stream << constraints.size();

    for(auto constraint : constraints)
    {
        readFrom(*constraint);
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(TemporalScenarioLayerModel& lm)
{
    int count;
    m_stream >> count;

    for(; count -- > 0;)
    {
        auto cstr = loadConstraintViewModel(*this, &lm);
        lm.addConstraintViewModel(cstr);
    }

    checkDelimiter();
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const TemporalScenarioLayerModel& lm)
{
    QJsonArray arr;

    for(auto cstrvm : constraintsViewModels(lm))
    {
        arr.push_back(toJsonObject(*cstrvm));
    }

    m_obj["Constraints"] = arr;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(TemporalScenarioLayerModel& lm)
{
    QJsonArray arr = m_obj["Constraints"].toArray();

    for(const auto& json_vref : arr)
    {
        Deserializer<JSONObject> deserializer {json_vref.toObject() };
        auto cstrvm = loadConstraintViewModel(deserializer,
                                                &lm);
        lm.addConstraintViewModel(cstrvm);
    }
}


#include <iscore/serialization/VisitorCommon.hpp>
void TemporalScenarioLayerModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}
