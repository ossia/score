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

    auto constraint = constraintPath.find<ConstraintModel>();
    m_oldEndDate = constraint->startDate() + constraint->defaultDuration();
    auto& endEvent = static_cast<ScenarioModel*>(constraint->parent())->event(constraint->endEvent());

    m_cmd = new MoveEvent(iscore::IDocument::path(constraint->parent()),
                          constraint->endEvent(),
                          constraint->startDate() + duration,
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
