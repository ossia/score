#include "CreateCurvesFromAddresses.hpp"
#include "base/plugins/iscore-plugin-scenario/Scenario/Commands/Constraint/AddProcessToConstraint.hpp"
#include "base/plugins/iscore-plugin-scenario/Scenario/Document/Constraint/ConstraintModel.hpp"
#include <Automation/AutomationModel.hpp>

using namespace iscore;

CreateCurvesFromAddresses::CreateCurvesFromAddresses(
        Path<ConstraintModel>&& constraint,
        const QList<Address>& addresses):
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_path {constraint},
    m_addresses {addresses}
{
    for(int i = 0; i < m_addresses.size(); ++i)
    {
        auto cmd = new Scenario::Command::AddProcessToConstraint{
                   Path<ConstraintModel>{m_path},
                   "Automation"};
        m_serializedCommands.push_back(cmd->serialize());
        delete cmd;
    }
}

void CreateCurvesFromAddresses::undo() const
{
    for(auto& cmd_pack : m_serializedCommands)
    {
        auto cmd = new Scenario::Command::AddProcessToConstraint;
        cmd->deserialize(cmd_pack);
        cmd->undo();

        delete cmd;
    }
}

void CreateCurvesFromAddresses::redo() const
{
    auto& constraint = m_path.find();

    for(int i = 0; i < m_addresses.size(); ++i)
    {
        // Creation
        auto cmd = new Scenario::Command::AddProcessToConstraint;
        cmd->deserialize(m_serializedCommands[i]);
        cmd->redo();

        // Change the address
        // TODO maybe pass parameters to AddProcessToConstraint?
        // Or do an overloaded command ?
        auto id = cmd->processId();

        auto& curve = safe_cast<AutomationModel&>(constraint.processes.at(id));
        curve.setAddress(m_addresses[i]);

        delete cmd;
    }
}

void CreateCurvesFromAddresses::serializeImpl(QDataStream& s) const
{
    s << m_path << m_addresses << m_serializedCommands;
}

void CreateCurvesFromAddresses::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_addresses >> m_serializedCommands;
}
