#include "MoveEvent.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

#include <core/application/Application.hpp>
#include <core/view/View.hpp>

#include <QApplication>
#define CMD_UID 1200

using namespace iscore;
using namespace Scenario::Command;

MoveEvent::MoveEvent() :
    SerializableCommand {"ScenarioControl",
    "MoveEvent",
    QObject::tr("Event move")
}
{
}

MoveEvent::MoveEvent(ObjectPath&& scenarioPath, EventData data) :
    SerializableCommand {"ScenarioControl",
    "MoveEvent",
    QObject::tr("Event move")
},
m_path {std::move(scenarioPath) },
m_eventId {data.eventClickedId},
m_newHeightPosition {data.relativeY},
m_newX {data.dDate}
{
    auto scenar = m_path.find<ScenarioModel>();
    auto ev = scenar->event(m_eventId);
    m_oldHeightPosition = ev->heightPercentage();
    m_oldX = ev->date();
}

void MoveEvent::undo()
{
    auto scenar = m_path.find<ScenarioModel>();

    StandardDisplacementPolicy::setEventPosition(*scenar,
            m_eventId,
            m_oldX,
            m_oldHeightPosition);
}

void MoveEvent::redo()
{
    auto scenar = m_path.find<ScenarioModel>();

    StandardDisplacementPolicy::setEventPosition(*scenar,
            m_eventId,
            m_newX,
            m_newHeightPosition);
}

int MoveEvent::id() const
{
    return canMerge() ? CMD_UID : -1;
}

bool MoveEvent::mergeWith(const QUndoCommand* other)
{
    // Maybe set m_mergeable = false at the end ?
    if(other->id() != id())
    {
        return false;
    }

    auto cmd = static_cast<const MoveEvent*>(other);
    m_newX = cmd->m_newX;
    m_newHeightPosition = cmd->m_newHeightPosition;

    return true;
}

void MoveEvent::serializeImpl(QDataStream& s) const
{
    s << m_path << m_eventId
      << m_oldHeightPosition << m_newHeightPosition << m_oldX << m_newX;
}

void MoveEvent::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_eventId
      >> m_oldHeightPosition >> m_newHeightPosition >> m_oldX >> m_newX;
}
