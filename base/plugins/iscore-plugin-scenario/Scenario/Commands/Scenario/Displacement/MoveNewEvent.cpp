#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <boost/core/explicit_operator_bool.hpp>
#include <qbytearray.h>
#include <algorithm>

#include "MoveNewEvent.hpp"
#include "Scenario/Commands/Scenario/Displacement/MoveEventOnCreationMeta.hpp"
#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/tools/ModelPathSerialization.hpp"


using namespace iscore;
using namespace Scenario::Command;

MoveNewEvent::MoveNewEvent(
        Path<Scenario::ScenarioModel>&& scenarioPath,
        const Id<ConstraintModel>& constraintId,
        const Id<EventModel>& eventId,
        const TimeValue& date,
        const double y,
        bool yLocked) :
    m_path {std::move(scenarioPath)},
    m_constraintId {constraintId},
    m_cmd{Path<Scenario::ScenarioModel>{m_path}, eventId, date, ExpandMode::Scale},
    m_y{y},
    m_yLocked{yLocked}
{

}

MoveNewEvent::MoveNewEvent(
        Path<Scenario::ScenarioModel>&& scenarioPath,
        const Id<ConstraintModel>& constraintId,
        const Id<EventModel>& eventId,
        const TimeValue& date,
        const double y,
        bool yLocked,
        ExpandMode mode) :
    m_path {scenarioPath},
    m_constraintId {constraintId},
    m_cmd{std::move(scenarioPath), eventId, date, mode},
    m_y{y},
    m_yLocked{yLocked}
{

}

void MoveNewEvent::undo() const
{
    m_cmd.undo();
    // It is not needed to move the constraint since
    // the sub-command already does it correctly.
}

void MoveNewEvent::redo() const
{
    m_cmd.redo();
    if(! m_yLocked)
    {
        updateConstraintVerticalPos(
                    m_y,
                    m_constraintId,
                    m_cmd.path().find());
    }
}

void MoveNewEvent::serializeImpl(DataStreamInput & s) const
{
    s << m_path << m_cmd.serialize() << m_constraintId << m_y << m_yLocked;
}

void MoveNewEvent::deserializeImpl(DataStreamOutput & s)
{
    QByteArray a;
    s >> m_path >> a >> m_constraintId >> m_y >> m_yLocked;

    m_cmd.deserialize(a);
}
