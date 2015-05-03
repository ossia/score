#include "RemoveProcessFromConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"


#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "source/ProcessInterfaceSerialization/ProcessModelSerialization.hpp"
#include "source/ProcessInterfaceSerialization/ProcessViewModelInterfaceSerialization.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveProcessFromConstraint::RemoveProcessFromConstraint(ObjectPath&& constraintPath,
        id_type<ProcessModel> processId) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {std::move(constraintPath) },
    m_processId {processId}
{
    auto& constraint = m_path.find<ConstraintModel>();

    // Save the process
    Serializer<DataStream> s1{&m_serializedProcessData};
    auto proc = constraint.process(m_processId);
    s1.readFrom(*proc);

    // Save ALL the view models!
    for(const auto& viewmodel : proc->viewModels())
    {
        QByteArray vm_arr;
        Serializer<DataStream> s{&vm_arr};
        s.readFrom(*viewmodel);

        m_serializedViewModels[identifierOfProcessViewModelFromConstraint(viewmodel)] = vm_arr;
    }
}

void RemoveProcessFromConstraint::undo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    Deserializer<DataStream> s {m_serializedProcessData};
    constraint.addProcess(createProcess(s, &constraint));

    // Restore the view models
    for(auto it = m_serializedViewModels.begin(); it != m_serializedViewModels.end(); ++it)
    {
        auto deck = constraint
                .box(id_type<BoxModel>(std::get<0>(it.key())))
                ->deck(id_type<DeckModel>(std::get<1>(it.key())));

        Deserializer<DataStream> s {it.value()};
        auto pvm = createProcessViewModel(s,
                                          deck->parentConstraint(),
                                          deck);
        deck->addProcessViewModel(pvm);
    }
}

void RemoveProcessFromConstraint::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();
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
