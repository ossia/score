#include "Process/AbstractScenarioViewModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModelSerialization.hpp"
#include "TemporalScenarioViewModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const TemporalScenarioViewModel& pvm)
{
    auto constraints = constraintsViewModels(pvm);

    m_stream << constraints.size();

    for(auto constraint : constraints)
    {
        readFrom(*constraint);
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(TemporalScenarioViewModel& pvm)
{
    int count;
    m_stream >> count;

    for(; count -- > 0;)
    {
        auto cstr = loadConstraintViewModel(*this, &pvm);
        pvm.addConstraintViewModel(cstr);
    }

    checkDelimiter();
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const TemporalScenarioViewModel& pvm)
{
    QJsonArray arr;

    for(auto cstrvm : constraintsViewModels(pvm))
    {
        arr.push_back(toJsonObject(*cstrvm));
    }

    m_obj["Constraints"] = arr;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(TemporalScenarioViewModel& pvm)
{
    QJsonArray arr = m_obj["Constraints"].toArray();

    for(const auto& json_vref : arr)
    {
        Deserializer<JSONObject> deserializer {json_vref.toObject() };
        auto cstrvm = loadConstraintViewModel(deserializer,
                                                &pvm);
        pvm.addConstraintViewModel(cstrvm);
    }
}


#include <iscore/serialization/VisitorCommon.hpp>
void TemporalScenarioViewModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}
