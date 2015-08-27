#include "RemoveProcessFromConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"


#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/LayerModel.hpp"
#include "source/ProcessInterfaceSerialization/ProcessModelSerialization.hpp"
#include "source/ProcessInterfaceSerialization/LayerModelSerialization.hpp"

#include <iscore/document/DocumentInterface.hpp>

using namespace iscore;
using namespace Scenario::Command;

RemoveProcessFromConstraint::RemoveProcessFromConstraint(
        ModelPath<ConstraintModel>&& constraintPath,
        const id_type<Process>& processId) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_path {std::move(constraintPath) },
    m_processId {processId}
{
    auto& constraint = m_path.find();

    // Save the process
    Serializer<DataStream> s1{&m_serializedProcessData};
    auto& proc = constraint.process(m_processId);
    s1.readFrom(proc);

    // Save ALL the view models!
    for(const auto& layer : proc.layers())
    {
        QByteArray vm_arr;
        Serializer<DataStream> s{&vm_arr};
        s.readFrom(*layer);

        m_serializedViewModels.append({iscore::IDocument::safe_path(*layer), vm_arr});
    }
}

void RemoveProcessFromConstraint::undo()
{
    auto& constraint = m_path.find();
    Deserializer<DataStream> s {m_serializedProcessData};
    constraint.addProcess(createProcess(s, &constraint));

    // Restore the view models
    for(const auto& it : m_serializedViewModels)
    {
        const auto& path = it.first.unsafePath().vec();

        auto& slot = constraint
                .rack(id_type<RackModel>(path.at(path.size() - 3).id()))
                .slot(id_type<SlotModel>(path.at(path.size() - 2).id()));

        Deserializer<DataStream> s {it.second};
        auto lm = createLayerModel(s,
                                   slot.parentConstraint(),
                                   &slot);
        slot.addLayerModel(lm);
    }
}

void RemoveProcessFromConstraint::redo()
{
    auto& constraint = m_path.find();
    constraint.removeProcess(m_processId);

    // The view models will be deleted accordingly.
}

void RemoveProcessFromConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_processId << m_serializedProcessData << m_serializedViewModels;
}

void RemoveProcessFromConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_processId >> m_serializedProcessData >> m_serializedViewModels;
}
