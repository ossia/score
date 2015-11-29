#include <Automation/AutomationModel.hpp>
#include <boost/optional/optional.hpp>
#include <qdatastream.h>
#include <qglobal.h>

#include "Automation/AutomationProcessMetadata.hpp"
#include "CreateCurvesFromAddresses.hpp"
#include "Process/Process.hpp"
#include "State/Address.hpp"
#include "base/plugins/iscore-plugin-scenario/Scenario/Commands/Constraint/AddProcessToConstraint.hpp"
#include "base/plugins/iscore-plugin-scenario/Scenario/Document/Constraint/ConstraintModel.hpp"
#include "core/application/ApplicationComponents.hpp"
#include "core/application/ApplicationContext.hpp"
#include "iscore/command/CommandData.hpp"
#include "iscore/command/SerializableCommand.hpp"
#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/tools/ModelPath.hpp"
#include "iscore/tools/ModelPathSerialization.hpp"
#include "iscore/tools/NotifyingMap.hpp"
#include "iscore/tools/SettableIdentifier.hpp"
#include "iscore/tools/std/StdlibWrapper.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

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
