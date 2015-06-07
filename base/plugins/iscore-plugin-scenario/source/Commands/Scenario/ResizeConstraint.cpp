#include "ResizeConstraint.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "iscore/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;

ResizeConstraint::ResizeConstraint() :
    SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
    m_cmd {new MoveEvent}
{

}

ResizeConstraint::~ResizeConstraint()
{
    delete m_cmd;
}

ResizeConstraint::ResizeConstraint(ObjectPath&& constraintPath,
                                   TimeValue duration,
                                   ExpandMode mode) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()}
{
    auto& constraint = constraintPath.find<ConstraintModel>();
    auto& endEvent = static_cast<ScenarioModel*>(constraint.parent())->event(constraint.endEvent());

    m_endEvent = constraint.endEvent();
    m_constraintStartDate = constraint.startDate();
    m_endEventHeightPercentage = endEvent.heightPercentage();

    m_cmd = new MoveEvent(iscore::IDocument::path(constraint.parent()),
                          constraint.endEvent(),
                          constraint.startDate() + duration,
                          endEvent.heightPercentage(),
                          mode);
}

void ResizeConstraint::undo()
{
    m_cmd->undo();
}

void ResizeConstraint::redo()
{
    m_cmd->redo();
}

void ResizeConstraint::update(const ObjectPath& constraintPath, const TimeValue& duration, ExpandMode mode)
{
    m_cmd->update(constraintPath,
                  m_endEvent,
                  m_constraintStartDate + duration,
                  m_endEventHeightPercentage,
                  mode);
}


void ResizeConstraint::serializeImpl(QDataStream& s) const
{
    s << m_cmd->serialize() << m_endEvent << m_constraintStartDate << m_endEventHeightPercentage;
}

void ResizeConstraint::deserializeImpl(QDataStream& s)
{
    QByteArray b;
    s >> b >> m_endEvent >> m_constraintStartDate >> m_endEventHeightPercentage;
    m_cmd->deserialize(b);
}
