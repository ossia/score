#include "CreateCurvesFromAddresses.hpp"
#include "base/plugins/iscore-plugin-scenario/source/Commands/Constraint/AddProcessToConstraint.hpp"
#include "base/plugins/iscore-plugin-scenario/source/Document/Constraint/ConstraintModel.hpp"
#include "base/plugins/iscore-plugin-curve/Automation/AutomationModel.hpp"

using namespace iscore;

#define CMD_UID 4000
#define CMD_NAME "CreateCurvesFromAddresses"
#define CMD_DESC QObject::tr("Create curves from addresses")

CreateCurvesFromAddresses::CreateCurvesFromAddresses() :
    SerializableCommand {"IScoreCohesionControl",
    CMD_NAME,
    CMD_DESC
}
{
}

CreateCurvesFromAddresses::CreateCurvesFromAddresses(ObjectPath&& constraint,
        const QList<Address>& addresses) :
    SerializableCommand {"IScoreCohesionControl",
    CMD_NAME,
    CMD_DESC
},
m_path {constraint},
m_addresses {addresses}
{
    for(int i = 0; i < m_addresses.size(); ++i)
    {
        auto cmd = new Scenario::Command::AddProcessToConstraint
        {
            ObjectPath{m_path},
            "Automation"
        };
        m_serializedCommands.push_back(cmd->serialize());
        delete cmd;
    }
}

void CreateCurvesFromAddresses::undo()
{
    for(auto& cmd_pack : m_serializedCommands)
    {
        auto cmd = new Scenario::Command::AddProcessToConstraint;
        cmd->deserialize(cmd_pack);
        cmd->undo();

        delete cmd;
    }
}

void CreateCurvesFromAddresses::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();

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

        auto curve = static_cast<AutomationModel*>(constraint.process(id));
        curve->setAddress(m_addresses[i]);

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
