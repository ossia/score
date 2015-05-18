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
                           double height,
                           ExpandMode mode):
    SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
   m_cmd{new MoveEvent{std::move(scenarioPath), eventId, date, height, mode}}
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
