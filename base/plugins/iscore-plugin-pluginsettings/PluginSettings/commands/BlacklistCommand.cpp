#include <QDebug>

#include "BlacklistCommand.hpp"
#include <iscore/command/SerializableCommand.hpp>

class DataStreamInput;
class DataStreamOutput;

using namespace iscore;

namespace PluginSettings
{
BlacklistCommand::BlacklistCommand(QString name, bool value) :
m_blacklistedState {{name, value}}
{

}

void BlacklistCommand::undo() const
{
}

void BlacklistCommand::redo() const
{
    ISCORE_TODO;
    /*
    QSettings s;

    auto currentList = s.value("PluginSettings/Blacklist", QStringList {}).toStringList();

    for(auto& elt : currentList)
    {
        if(!m_blacklistedState.contains(elt))
        {
            m_blacklistedState[elt] = true;
        }
    }

    QStringList newList;

    for(auto& key : m_blacklistedState.keys())
    {
        if(m_blacklistedState[key] == true)
        {
            newList.push_back(key);
        }
    }

    s.setValue("PluginSettings/Blacklist", newList);
    */
}

void BlacklistCommand::serializeImpl(DataStreamInput&) const { }

void BlacklistCommand::deserializeImpl(DataStreamOutput&) { }

/*
bool BlacklistCommand::mergeWith(const Command* other)
{
    // TODO

    //if(other->uid() != uid())   // make sure other is also an AppendText command
    //{
    //    return false;
    //}

    auto cmd = static_cast<const BlacklistCommand*>(other);

    for(auto& key : cmd->m_blacklistedState.keys())
    {
        m_blacklistedState[key] = cmd->m_blacklistedState[key];
    }

    return true;
}
*/


const CommandParentFactoryKey& BlacklistCommand::parentKey() const
{
    static CommandParentFactoryKey p;
    ISCORE_TODO;
    return p;
}

const CommandFactoryKey& BlacklistCommand::key() const
{
    static CommandFactoryKey p;
    ISCORE_TODO;
    return p;
}

QString BlacklistCommand::description() const
{
    ISCORE_TODO;
    return {};
}
}
