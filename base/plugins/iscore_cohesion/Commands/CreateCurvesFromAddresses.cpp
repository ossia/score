#include "CreateCurvesFromAddresses.hpp"
#include "../scenario/source/Commands/Constraint/AddProcessToConstraint.hpp"
#include "../scenario/source/Document/Constraint/ConstraintModel.hpp"
#include "../curve_plugin/Automation/AutomationModel.hpp"

#include <core/interface/presenter/PresenterInterface.hpp>
#include <QApplication>

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

CreateCurvesFromAddresses::CreateCurvesFromAddresses (ObjectPath&& constraint,
        QStringList addresses) :
    SerializableCommand {"IScoreCohesionControl",
    CMD_NAME,
    CMD_DESC
},
m_path {constraint},
m_addresses {addresses}
{
    for (int i = 0; i < m_addresses.size(); ++i)
    {
        auto cmd = new Scenario::Command::AddProcessToConstraint
        {
            ObjectPath{m_path},
            "Automation"
        };
        m_serializedCommands.push_back (cmd->serialize() );
        delete cmd;
    }
}

void CreateCurvesFromAddresses::undo()
{
    for (auto& cmd_pack : m_serializedCommands)
    {
        auto cmd = IPresenter::instantiateUndoCommand ("ScenarioControl",
                   "AddProcessToConstraint",
                   cmd_pack);
        cmd->undo();

        delete cmd;
    }
}

void CreateCurvesFromAddresses::redo()
{
    auto constraint = m_path.find<ConstraintModel>();

    for (int i = 0; i < m_addresses.size(); ++i)
    {
        // Creation
        auto cmd = IPresenter::instantiateUndoCommand ("ScenarioControl",
                   "AddProcessToConstraint",
                   m_serializedCommands[i]);

        cmd->redo();

        // Change the address
        // TODO maybe pass parameters to AddProcessToConstraint?
        auto addProcessCmd = static_cast<Scenario::Command::AddProcessToConstraint*> (cmd);
        auto id = addProcessCmd->processId();

        auto curve = static_cast<AutomationModel*> (constraint->process (id) );
        curve->setAddress (m_addresses[i]);

        delete cmd;
    }
}

int CreateCurvesFromAddresses::id() const
{
    return CMD_UID;
}

bool CreateCurvesFromAddresses::mergeWith (const QUndoCommand* other)
{
    return false;
}

void CreateCurvesFromAddresses::serializeImpl (QDataStream& s) const
{
    s << m_path << m_addresses << m_serializedCommands;
}

void CreateCurvesFromAddresses::deserializeImpl (QDataStream& s)
{
    s >> m_path >> m_addresses >> m_serializedCommands;
}
