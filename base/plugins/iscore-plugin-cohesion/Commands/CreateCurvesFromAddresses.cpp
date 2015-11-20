#include "CreateCurvesFromAddresses.hpp"
#include "base/plugins/iscore-plugin-scenario/Scenario/Commands/Constraint/AddProcessToConstraint.hpp"
#include "base/plugins/iscore-plugin-scenario/Scenario/Document/Constraint/ConstraintModel.hpp"
#include <Automation/AutomationModel.hpp>

using namespace iscore;

CreateCurvesFromAddresses::CreateCurvesFromAddresses(
        const ConstraintModel& constraint,
        const QList<Address>& addresses):
    m_path {constraint},
    m_addresses {addresses}
{
    // OPTIMIZEME write them on the stack instead
    m_serializedCommands.reserve(m_addresses.size());
    for(int i = 0; i < m_addresses.size(); ++i)
    {
        auto cmd = Scenario::Command::make_AddProcessToConstraint(
                   constraint,
                   AutomationProcessMetadata::factoryKey());
        m_serializedCommands.emplace_back(*cmd);
        delete cmd;
    }
}

void CreateCurvesFromAddresses::undo() const
{
    for(auto& cmd_pack : m_serializedCommands)
    {
        auto cmd = context.components.instantiateUndoCommand(cmd_pack);

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

        auto base_cmd = context.components.instantiateUndoCommand(m_serializedCommands[i]);

        auto cmd = safe_cast<Scenario::Command::AddProcessToConstraintBase*>(base_cmd);

        // Change the address
        // TODO maybe pass parameters to AddProcessToConstraint?
        // Or do an overloaded command ?
        auto id = cmd->processId();

        auto& curve = safe_cast<AutomationModel&>(constraint.processes.at(id));
        curve.setAddress(m_addresses[i]);

        delete cmd;
    }
}

// MOVEME
template<>
void Visitor<Reader<DataStream>>::readFrom(const CommandData& d)
{
    m_stream << d.parentKey << d.commandKey << d.data;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(CommandData& d)
{
    m_stream >> d.parentKey >> d.commandKey >> d.data;
    checkDelimiter();
}

void CreateCurvesFromAddresses::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_addresses;
    Visitor<Reader<DataStream>> v{s.stream().device()};
    readFrom_vector_obj_impl(v, m_serializedCommands);
}

void CreateCurvesFromAddresses::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_addresses;

    Visitor<Writer<DataStream>> v{s.stream().device()};
    writeTo_vector_obj_impl(v, m_serializedCommands);
}
