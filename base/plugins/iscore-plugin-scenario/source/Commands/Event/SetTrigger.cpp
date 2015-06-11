#include "SetTrigger.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "iscore/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;


SetTrigger::SetTrigger(ObjectPath&& eventPath, QString message) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {std::move(eventPath) },
m_trigger(message)
{
    auto& event = m_path.find<EventModel>();
    m_previousTrigger = event.trigger();

    if(m_previousTrigger.isEmpty() != m_trigger.isEmpty())
    {
        ScenarioModel* scenar = event.parentScenario();
        for (auto cstr : event.previousConstraints())
        {
            m_cmd.push_back( new SetRigidity(iscore::IDocument::path(scenar->constraint(cstr)), m_trigger.isEmpty()));
        }
    }
}

void SetTrigger::undo()
{
    auto& event = m_path.find<EventModel>();
    event.setTrigger(m_previousTrigger);

    for (auto cmd : m_cmd)
    {
        cmd->undo();
    }
}

void SetTrigger::redo()
{
    auto& event = m_path.find<EventModel>();
    event.setTrigger(m_trigger);

    for (auto cmd : m_cmd)
    {
        cmd->redo();
    }
}

void SetTrigger::serializeImpl(QDataStream& s) const
{
    s << m_path << m_trigger << m_previousTrigger;
}

void SetTrigger::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_trigger >> m_previousTrigger;
}
