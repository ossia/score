#include "Process/AbstractScenarioViewModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModelSerialization.hpp"
#include "TemporalScenarioViewModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const TemporalScenarioViewModel& pvm)
{
    auto constraints = constraintsViewModels(pvm);

    m_stream << (int) constraints.size();

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
        auto cstr = createConstraintViewModel(*this, &pvm);
        pvm.addConstraintViewModel(cstr);
    }

    checkDelimiter();
}



template<>
void Visitor<Reader<JSON>>::readFrom(const TemporalScenarioViewModel& pvm)
{
    QJsonArray arr;

    for(auto cstrvm : constraintsViewModels(pvm))
    {
        arr.push_back(toJsonObject(*cstrvm));
    }

    m_obj["Constraints"] = arr;
}

template<>
void Visitor<Writer<JSON>>::writeTo(TemporalScenarioViewModel& pvm)
{
    QJsonArray arr = m_obj["Constraints"].toArray();

    for(auto json_vref : arr)
    {
        Deserializer<JSON> deserializer {json_vref.toObject() };
        auto cstrvm = createConstraintViewModel(deserializer,
                                                &pvm);
        pvm.addConstraintViewModel(cstrvm);
    }
}



void TemporalScenarioViewModel::serialize(const VisitorVariant& vis) const
{
    if(vis.identifier == DataStream::type())
    {
        static_cast<DataStream::Serializer*>(vis.visitor)->readFrom(*this);
        return;
    }
    else if(vis.identifier == JSON::type())
    {
        static_cast<JSON::Serializer*>(vis.visitor)->readFrom(*this);
        return;
    }

    throw std::runtime_error("ScenarioViewModel only supports DataStream serialization");
}
