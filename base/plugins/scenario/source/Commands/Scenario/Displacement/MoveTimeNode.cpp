#include "MoveTimeNode.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;

MoveTimeNode::MoveTimeNode():
    iscore::SerializableCommand{
        "ScenarioControl",
        className(),
        description()} ,
    m_cmd{new MoveEvent}
{
}

MoveTimeNode::MoveTimeNode(ObjectPath&& scenarioPath,
                           id_type<EventModel> eventId,
                           const TimeValue& date,
                           double height):
    SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
   m_cmd{new MoveEvent{std::move(scenarioPath), eventId, date, height}}
{
}

MoveTimeNode::~MoveTimeNode()
{
    delete m_cmd;
}

void MoveTimeNode::undo()
{
    m_cmd->undo();
}

void MoveTimeNode::redo()
{
    m_cmd->redo();
}

bool MoveTimeNode::mergeWith(const Command* other)
{
    // Maybe set m_mergeable = false at the end ?
    if(other->uid() != uid())
    {
        return false;
    }

    auto otherMove = static_cast<const MoveTimeNode*>(other);
    m_cmd->mergeWith(otherMove->m_cmd);

    return true;
}

void MoveTimeNode::serializeImpl(QDataStream& s) const
{
    s << m_cmd->serialize();
}

void MoveTimeNode::deserializeImpl(QDataStream& s)
{
    QByteArray a;
    s >> a;

    m_cmd->deserialize(a);
}
