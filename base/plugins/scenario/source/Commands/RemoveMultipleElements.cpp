#include "RemoveMultipleElements.hpp"
#include <QApplication>

#include <core/interface/presenter/PresenterInterface.hpp>

using namespace iscore;
using namespace Scenario::Command;

RemoveMultipleElements::RemoveMultipleElements() :
    SerializableCommand {"ScenarioControl",
    "RemoveMultipleElements",
    QObject::tr("Group deletion")
}
{
}

RemoveMultipleElements::RemoveMultipleElements(
    QVector<SerializableCommand*> deletionCommands) :
    SerializableCommand {"ScenarioControl",
    "RemoveMultipleElements",
    QObject::tr("Group deletion")
}
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

int RemoveMultipleElements::id() const
{
    return 1;
}

bool RemoveMultipleElements::mergeWith(const QUndoCommand* other)
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
