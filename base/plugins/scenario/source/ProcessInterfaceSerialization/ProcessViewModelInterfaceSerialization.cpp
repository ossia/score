#include "ProcessViewModelInterfaceSerialization.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include <iscore/serialization/JSONValueVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const ProcessViewModelInterface& processViewModel)
{
    // To allow recration using createProcessViewModel.
    // This supposes that the process is stored inside a Constraint.
    m_stream << processViewModel.sharedProcessModel().id();

    readFrom(static_cast<const IdentifiedObject<ProcessViewModelInterface>&>(processViewModel));

    // ProcessViewModelInterface doesn't have any particular data to save

    // Save the subclass
    processViewModel.serialize(toVariant());

    insertDelimiter();
}

template<>
ProcessViewModelInterface* createProcessViewModel(Deserializer<DataStream>& deserializer,
        const ConstraintModel& constraint,
        QObject* parent)
{
    id_type<ProcessModel> sharedProcessId;
    deserializer.m_stream >> sharedProcessId;

    auto process = constraint.process(sharedProcessId);
    auto viewmodel = process->loadViewModel(deserializer.toVariant(),
                                            parent);

    deserializer.checkDelimiter();

    return viewmodel;
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const ProcessViewModelInterface& processViewModel)
{
    // To allow recration using createProcessViewModel.
    // This supposes that the process is stored inside a Constraint.
    m_obj["SharedProcessId"] = toJsonValue(processViewModel.sharedProcessModel().id());

    readFrom(static_cast<const IdentifiedObject<ProcessViewModelInterface>&>(processViewModel));

    // ProcessViewModelInterface doesn't have any particular data to save

    // Save the subclass
    processViewModel.serialize(toVariant());
}

template<>
ProcessViewModelInterface* createProcessViewModel(
        Deserializer<JSONObject>& deserializer,
        const ConstraintModel& constraint,
        QObject* parent)
{
    auto process = constraint.process(
                fromJsonValue<id_type<ProcessModel>>(deserializer.m_obj["SharedProcessId"]));
    auto viewmodel = process->loadViewModel(deserializer.toVariant(),
                                            parent);

    return viewmodel;
}
