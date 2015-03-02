#include "CreateEvent.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventData.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

CreateEvent::CreateEvent() :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_cmd {new CreateEventAfterEvent}
{

}

CreateEvent::~CreateEvent()
{
    delete m_cmd;
}

CreateEvent::CreateEvent(ObjectPath&& scenarioPath, EventData data) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()}
{
    auto scenar = scenarioPath.find<ScenarioModel>();

    data.eventClickedId = scenar->startEvent()->id();


    m_cmd = new CreateEventAfterEvent{std::move(scenarioPath), data};
}

void CreateEvent::undo()
{
    m_cmd->undo();
}

void CreateEvent::redo()
{
    m_cmd->redo();
}

int CreateEvent::id() const
{
    return canMerge() ? uid() : -1;
}

bool CreateEvent::mergeWith(const QUndoCommand* other)
{
    // Maybe set m_mergeable = false at the end ?
    if(other->id() != id())
    {
        return false;
    }

    auto cmd = static_cast<const CreateEvent*>(other);
    m_cmd->mergeWith(cmd->m_cmd);

    return true;
}

void CreateEvent::serializeImpl(QDataStream& s) const
{
    s << m_cmd->serialize();
}

void CreateEvent::deserializeImpl(QDataStream& s)
{
    QByteArray b;
    s >> b;
    m_cmd->deserialize(b);
}
