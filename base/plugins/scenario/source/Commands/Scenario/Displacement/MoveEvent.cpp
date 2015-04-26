#include "MoveEvent.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;

MoveEvent::MoveEvent(ObjectPath&& scenarioPath,
                     id_type<EventModel> eventId,
                     const TimeValue& date,
                     double height,
                     ExpandMode mode) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {std::move(scenarioPath)},
    m_eventId {eventId},
    m_newHeightPosition {height},
    m_newDate {date},
    m_mode{mode}
{
    auto scenar = m_path.find<ScenarioModel>();
    auto ev = scenar->event(m_eventId);
    m_oldHeightPosition = ev->heightPercentage();
    m_oldDate = ev->date();
}

void MoveEvent::undo()
{
    auto scenar = m_path.find<ScenarioModel>();
    auto event = scenar->event(m_eventId);

    event->setHeightPercentage(m_oldHeightPosition);
    StandardDisplacementPolicy::updatePositions(*scenar,
                                                m_movableTimenodes,
                                                m_oldDate - event->date(),
                                                [&] (ProcessSharedModelInterface* p, const TimeValue& t)
    { p->expandProcess(m_mode, t); });
}

void MoveEvent::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    auto event = scenar->event(m_eventId);

    auto timeNode = scenar->event(m_eventId)->timeNode();
    StandardDisplacementPolicy::getRelatedElements(*scenar,
                                                   timeNode,
                                                   m_movableTimenodes);
    event->setHeightPercentage(m_newHeightPosition);
    StandardDisplacementPolicy::updatePositions(*scenar,
                                                m_movableTimenodes,
                                                m_newDate - event->date(),
                                                [&] (ProcessSharedModelInterface* p, const TimeValue& t)
    { p->expandProcess(m_mode, t); });
}

bool MoveEvent::mergeWith(const Command* other)
{
    // Maybe set m_mergeable = false at the end ?
    if(other->uid() != uid())
    {
        return false;
    }

    auto cmd = static_cast<const MoveEvent*>(other);
    m_newDate = cmd->m_newDate;
    m_newHeightPosition = cmd->m_newHeightPosition;
    for (auto tn : cmd->m_movableTimenodes)
    {
        m_movableTimenodes.push_back(tn);
    }

    return true;
}

void MoveEvent::serializeImpl(QDataStream& s) const
{
    s << m_path << m_eventId
      << m_oldHeightPosition << m_newHeightPosition << m_oldDate << m_newDate
      << m_movableTimenodes << (int)m_mode;
}

void MoveEvent::deserializeImpl(QDataStream& s)
{
    int mode;
    s >> m_path >> m_eventId
      >> m_oldHeightPosition >> m_newHeightPosition >> m_oldDate >> m_newDate
      >> m_movableTimenodes >> mode;
    m_mode = static_cast<ExpandMode>(mode);
}
