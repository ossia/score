#include "SetTrigger.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "iscore/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;


SetTrigger::SetTrigger(Path<EventModel>&& eventPath, QString message) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
m_path {std::move(eventPath) },
m_trigger(message)
{
    auto& event = m_path.find();
    m_previousTrigger = event.trigger();
}

SetTrigger::~SetTrigger()
{
    qDeleteAll(m_cmds);
}

void SetTrigger::undo()
{
    auto& event = m_path.find();
    event.setTrigger(m_previousTrigger);

    for (auto cmd : m_cmds)
    {
        cmd->undo();
        delete cmd;
    }
}

void SetTrigger::redo()
{
    ISCORE_TODO;
    /*
    auto& event = m_path.find();
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
    */
}

void SetTrigger::serializeImpl(QDataStream& s) const
{
    s << m_path << m_trigger << m_previousTrigger;
    s << m_cmds.count();

    for(const auto& cmd : m_cmds)
    {
        s << cmd->serialize();
    }
}

void SetTrigger::deserializeImpl(QDataStream& s)
{
    int n;
    s >> m_path >> m_trigger >> m_previousTrigger >> n;

    for(;n-->0;)
    {
        QByteArray a;
        s >> a;
        auto cmd = new SetRigidity;
        cmd->deserialize(a);
        m_cmds.append(cmd);
    }
}
