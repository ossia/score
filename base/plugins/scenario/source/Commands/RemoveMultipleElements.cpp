#include "RemoveMultipleElements.hpp"
#include <QApplication>

#include <public_interface/presenter/PresenterInterface.hpp>

using namespace iscore;
using namespace Scenario::Command;

RemoveMultipleElements::RemoveMultipleElements(
    QVector<SerializableCommand*> deletionCommands) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()}
{
    for(auto& cmd : deletionCommands)
    {
        m_serializedCommands.push_back({{cmd->parentName(), cmd->name() }, cmd->serialize() });
        m_serializedCommandsDecreasing.push_front({{cmd->parentName(), cmd->name() }, cmd->serialize() });
    }
}

void RemoveMultipleElements::undo()
{
    for(auto& cmd_pack : m_serializedCommandsDecreasing)
    {
        // Put this in the ctor as an optimization
        auto cmd = IPresenter::instantiateUndoCommand(cmd_pack.first.first,
                   cmd_pack.first.second,
                   cmd_pack.second);

        cmd->undo();
    }
}

void RemoveMultipleElements::redo()
{
    for(auto& cmd_pack : m_serializedCommands)
    {
        // Put this in the ctor as an optimization
        auto cmd = IPresenter::instantiateUndoCommand(cmd_pack.first.first,
                   cmd_pack.first.second,
                   cmd_pack.second);

        cmd->redo();
    }
}

bool RemoveMultipleElements::mergeWith(const Command* other)
{
    return false;
}

void RemoveMultipleElements::serializeImpl(QDataStream& s) const
{
    s << m_serializedCommands;
}

void RemoveMultipleElements::deserializeImpl(QDataStream& s)
{
    s >> m_serializedCommands;
}
