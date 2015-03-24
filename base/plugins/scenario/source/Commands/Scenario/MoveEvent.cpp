#include "MoveEvent.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;

MoveEvent::MoveEvent(ObjectPath&& scenarioPath, EventData data) :
    SerializableCommand {"ScenarioControl",
                         "MoveEvent",
                         QObject::tr("Event move") },
    m_path {std::move(scenarioPath) },
    m_eventId {data.eventClickedId},
    m_newHeightPosition {data.relativeY},
    m_newX {data.dDate}
{
    auto scenar = m_path.find<ScenarioModel>();
    auto ev = scenar->event(m_eventId);
    m_oldHeightPosition = ev->heightPercentage();
    m_oldX = ev->date();
    m_already_moved_timeNodes.clear();
}

bool MoveEvent::init()
{
    auto scenar = m_path.find<ScenarioModel>();

    auto timeNode = scenar->event(m_eventId)->timeNode();

    StandardDisplacementPolicy::getRelatedElements(*scenar, timeNode, m_already_moved_timeNodes);
    return true;
}

bool MoveEvent::finish()
{
    return true;
}

void MoveEvent::undo()
{
    auto scenar = m_path.find<ScenarioModel>();
    auto event = scenar->event(m_eventId);
    auto deltaTime = m_oldX - event->date();
    event->setHeightPercentage(m_oldHeightPosition);
    StandardDisplacementPolicy::updatePositions(*scenar,
                                                m_already_moved_timeNodes,
                                                deltaTime,
                                                [] (ProcessSharedModelInterface* p, TimeValue t) { p->setDurationAndScale(t); });
}

void MoveEvent::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    auto event = scenar->event(m_eventId);
    auto deltaTime = m_newX - event->date();
    event->setHeightPercentage(m_newHeightPosition);
    StandardDisplacementPolicy::updatePositions(*scenar,
                                                m_already_moved_timeNodes,
                                                deltaTime,
                                                [] (ProcessSharedModelInterface* p, TimeValue t) { p->setDurationAndScale(t); });
}

bool MoveEvent::mergeWith(const Command* other)
{
    // Maybe set m_mergeable = false at the end ?
    if(other->uid() != uid())
    {
        return false;
    }

    auto cmd = static_cast<const MoveEvent*>(other);
    m_newX = cmd->m_newX;
    m_newHeightPosition = cmd->m_newHeightPosition;
    for (auto tn : cmd->m_already_moved_timeNodes)
    {
        m_already_moved_timeNodes.push_back(tn);
    }

    return true;
}

void MoveEvent::serializeImpl(QDataStream& s) const
{
    s << m_path << m_eventId
      << m_oldHeightPosition << m_newHeightPosition << m_oldX << m_newX
      << m_already_moved_timeNodes ;
}

void MoveEvent::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_eventId
      >> m_oldHeightPosition >> m_newHeightPosition >> m_oldX >> m_newX
      >> m_already_moved_timeNodes;
}
