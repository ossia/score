#include "RemoveProcessFromConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"

#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "source/ProcessInterfaceSerialization/ProcessSharedModelInterfaceSerialization.hpp"

#include <QDebug>

using namespace iscore;
using namespace Scenario::Command;

RemoveProcessFromConstraint::RemoveProcessFromConstraint(ObjectPath&& constraintPath,
        id_type<ProcessSharedModelInterface> processId) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {std::move(constraintPath) },
m_processId {processId}
{
    auto constraint = m_path.find<ConstraintModel>();

    Serializer<DataStream> s{&m_serializedProcessData};

    s.readFrom(*constraint->process(m_processId));
}

void RemoveProcessFromConstraint::undo()
{
    auto constraint = m_path.find<ConstraintModel>();
    Deserializer<DataStream> s {&m_serializedProcessData};
    constraint->addProcess(createProcess(s, constraint));
}

void RemoveProcessFromConstraint::redo()
{
    auto constraint = m_path.find<ConstraintModel>();
    constraint->removeProcess(m_processId);
}

bool RemoveProcessFromConstraint::mergeWith(const Command* other)
{
    return false;
}

void RemoveProcessFromConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_processId << m_serializedProcessData;
}

void RemoveProcessFromConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_processId >> m_serializedProcessData;
}
