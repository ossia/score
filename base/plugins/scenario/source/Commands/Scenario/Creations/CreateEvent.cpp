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

    if (data.endTimeNodeId.val() == -1)
    {
        m_cmd = new CreateEventAfterEvent{std::move(scenarioPath), data};
        m_tn = false;
    }
    else
    {
        m_tnCmd = new CreateEventAfterEventOnTimeNode{std::move(scenarioPath), data};
        m_tn = true;
    }
}

void CreateEvent::undo()
{
    if (m_tn)
        m_tnCmd->undo();
    else
        m_cmd->undo();
}

void CreateEvent::redo()
{
    if (m_tn)
        m_tnCmd->redo();
    else
        m_cmd->redo();
}

bool CreateEvent::mergeWith(const Command* other)
{
    // Maybe set m_mergeable = false at the end ?
    if(other->uid() != uid())
    {
        return false;
    }

    auto cmd = static_cast<const CreateEvent*>(other);
    m_cmd->mergeWith(cmd->m_cmd);

    return true;
}

void CreateEvent::serializeImpl(QDataStream& s) const
{
    s << m_cmd->serialize() << m_tnCmd->serialize() << m_tn;
}

void CreateEvent::deserializeImpl(QDataStream& s)
{
    QByteArray b, c;
    s >> b >> c >> m_tn;
    m_cmd->deserialize(b);
    m_tnCmd->deserialize(c);
}
