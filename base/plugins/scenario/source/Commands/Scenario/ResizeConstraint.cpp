#include "ResizeConstraint.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "core/interface/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

#include <ProcessInterface/TimeValue.hpp>
ResizeConstraint::ResizeConstraint() :
    SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
    m_cmd {new MoveEvent}
{

}

ResizeConstraint::~ResizeConstraint()
{
    // TODO gare aux fuite mémoire !
//	delete m_cmd;
}

ResizeConstraint::ResizeConstraint(ObjectPath&& constraintPath, TimeValue duration) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()}
{

    auto constraint = constraintPath.find<ConstraintModel>();
    EventData endEventData{};
    endEventData.dDate = constraint->startDate() + duration;
    endEventData.relativeY = constraint->heightPercentage();
    endEventData.eventClickedId = constraint->endEvent();

    m_oldEndDate = constraint->startDate() + constraint->defaultDuration();

    m_cmd = new MoveEvent{iscore::IDocument::path(constraint->parent()), endEventData};
}

void ResizeConstraint::undo()
{
    m_cmd->undo();
}

void ResizeConstraint::redo()
{
    m_cmd->redo();
}

int ResizeConstraint::id() const
{
    return uid();
}

bool ResizeConstraint::mergeWith(const QUndoCommand* other)
{
    if(other->id() != id())
    {
        return false;
    }

    delete m_cmd;
    auto cmd = static_cast<const ResizeConstraint*>(other);
    m_cmd = cmd->m_cmd;

    m_cmd->m_oldX = m_oldEndDate;

    return true;;
}

void ResizeConstraint::serializeImpl(QDataStream& s) const
{
    s << m_cmd->serialize() << m_oldEndDate;
}

void ResizeConstraint::deserializeImpl(QDataStream& s)
{
    QByteArray b;
    s >> b >> m_oldEndDate;
    m_cmd->deserialize(b);
}
