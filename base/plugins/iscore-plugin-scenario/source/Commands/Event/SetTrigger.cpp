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


}

void SetTrigger::undo()
{
    auto& event = m_path.find<EventModel>();
    event.setTrigger(m_previousTrigger);

    for (auto cmd : m_cmd)
    {
        cmd->undo();
        delete cmd;
    }
}

void SetTrigger::redo()
{
    auto& event = m_path.find<EventModel>();
    event.setTrigger(m_trigger);

    if(m_previousTrigger.isEmpty() != m_trigger.isEmpty())
    {
        ScenarioModel* scenar = event.parentScenario();
        for (auto cstr : event.previousConstraints())
        {
            auto cmd = new SetRigidity(iscore::IDocument::path(scenar->constraint(cstr)), m_trigger.isEmpty());
            cmd->redo();
            m_cmd.push_back(cmd);
        }
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
