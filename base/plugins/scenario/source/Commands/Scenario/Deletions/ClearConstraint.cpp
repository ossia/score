#include "ClearConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "source/ProcessInterfaceSerialization/ProcessSharedModelInterfaceSerialization.hpp"

using namespace iscore;
using namespace Scenario::Command;

ClearConstraint::ClearConstraint(ObjectPath&& constraintPath) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {std::move(constraintPath) }
{
    auto constraint = m_path.find<ConstraintModel>();

    for(const BoxModel* box : constraint->boxes())
    {
        QByteArray arr;
        Serializer<DataStream> s {&arr};
        s.readFrom(*box);
        m_serializedBoxes.push_back(arr);
    }

    for(const ProcessSharedModelInterface* process : constraint->processes())
    {
        QByteArray arr;
        Serializer<DataStream> s {&arr};
        s.readFrom(*process);
        m_serializedProcesses.push_back(arr);
    }

    // @todo save the mapping in the parent scenario view models.
}

void ClearConstraint::undo()
{
    auto constraint = m_path.find<ConstraintModel>();

    for(auto& serializedProcess : m_serializedProcesses)
    {
        Deserializer<DataStream> s {&serializedProcess};
        constraint->addProcess(createProcess(s, constraint));
    }

    for(auto& serializedBox : m_serializedBoxes)
    {
        Deserializer<DataStream> s {&serializedBox};
        constraint->addBox(new BoxModel {s, constraint});
    }
}

void ClearConstraint::redo()
{
    auto constraint = m_path.find<ConstraintModel>();

    for(auto& process : constraint->processes())
    {
        constraint->removeProcess(process->id());
    }

    for(auto& box : constraint->boxes())
    {
        constraint->removeBox(box->id());
    }
}

bool ClearConstraint::mergeWith(const Command* other)
{
    return false;
}

void ClearConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_serializedBoxes << m_serializedProcesses;
}

void ClearConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_serializedBoxes >> m_serializedProcesses;
}
