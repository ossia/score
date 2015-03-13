#include "CreatesStatesFromParameters.hpp"
#include "../scenario/source/Commands/Event/AddStateToEvent.hpp"
#include "../scenario/source/Document/Event/EventModel.hpp"


using namespace iscore;

#define CMD_UID 4000
#define CMD_NAME "CreateStatesFromParameters"
#define CMD_DESC QObject::tr("CreateStatesFromParameters")

CreateStatesFromParameters::CreateStatesFromParameters() :
    SerializableCommand {"IScoreCohesionControl",
                         CMD_NAME,
                         CMD_DESC}
{
}

CreateStatesFromParameters::CreateStatesFromParameters(ObjectPath&& event,
                                                       QList<Message> addresses) :
    SerializableCommand {"IScoreCohesionControl",
                         CMD_NAME,
                         CMD_DESC},
    m_path {event},
    m_messages {addresses}
{
    for(auto& message : m_messages)
    {
        auto cmd = new Scenario::Command::AddStateToEvent{
                   ObjectPath{m_path},
                   QString{"%1 %2"}
                    .arg(message.address)
                    .arg(message.value.toString())};
        m_serializedCommands.push_back(cmd->serialize());
        delete cmd;
    }
}

void CreateStatesFromParameters::undo()
{
    for(auto& cmd_pack : m_serializedCommands)
    {
        auto cmd = new Scenario::Command::AddStateToEvent;
        cmd->deserialize(cmd_pack);
        cmd->undo();

        delete cmd;
    }
}

void CreateStatesFromParameters::redo()
{
    for(auto& cmd_pack : m_serializedCommands)
    {
        auto cmd = new Scenario::Command::AddStateToEvent;
        cmd->deserialize(cmd_pack);
        cmd->redo();

        delete cmd;
    }
}

bool CreateStatesFromParameters::mergeWith(const Command* other)
{
    return false;
}

void CreateStatesFromParameters::serializeImpl(QDataStream& s) const
{
    s << m_path << m_messages << m_serializedCommands;
}

void CreateStatesFromParameters::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_messages >> m_serializedCommands;
}
