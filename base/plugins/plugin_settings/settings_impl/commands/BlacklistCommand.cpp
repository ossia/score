#include "BlacklistCommand.hpp"
#include <QSettings>
using namespace iscore;
#include <QDebug>

BlacklistCommand::BlacklistCommand (QString name, bool value) :
    SerializableCommand {"", "", ""},
m_blacklistedState {{name, value}}
{

}

void BlacklistCommand::undo()
{
}

void BlacklistCommand::redo()
{
    QSettings s;

    auto currentList = s.value ("PluginSettings/Blacklist", QStringList {}).toStringList();

    for (auto& elt : currentList)
    {
        if (!m_blacklistedState.contains (elt) )
        {
            m_blacklistedState[elt] = true;
        }
    }

    QStringList newList;

    for (auto& key : m_blacklistedState.keys() )
    {
        if (m_blacklistedState[key] == true)
        {
            newList.push_back (key);
        }
    }

    s.setValue ("PluginSettings/Blacklist", newList);
}

bool BlacklistCommand::mergeWith (const QUndoCommand* other)
{
    if (other->id() != id() ) // make sure other is also an AppendText command
    {
        return false;
    }

    auto cmd = static_cast<const BlacklistCommand*> (other);

    for (auto& key : cmd->m_blacklistedState.keys() )
    {
        m_blacklistedState[key] = cmd->m_blacklistedState[key];
    }

    return true;
}
