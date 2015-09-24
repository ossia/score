#include "SetTrigger.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/Trigger/TriggerModel.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioModel.hpp"

#include "iscore/document/DocumentInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;



// TODO
SetTrigger::SetTrigger(Path<TimeNodeModel>&& timeNodePath,
                       Trigger trigger) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
m_path {std::move(timeNodePath) },
m_trigger(std::move(trigger))
{
    auto& tn = m_path.find();
    m_previousTrigger = tn.trigger()->expression();
}

SetTrigger::~SetTrigger()
{
    /*
    qDeleteAll(m_cmds);
    */
}

void SetTrigger::undo()
{
    auto& tn = m_path.find();
    tn.trigger()->setExpression(m_previousTrigger);

    /*

    for (auto cmd : m_cmds)
    {
        cmd->undo();
        delete cmd;
    }
    */
}

void SetTrigger::redo()
{
    auto& tn = m_path.find();
    tn.trigger()->setExpression(m_trigger);

/*    if(m_previousTrigger.isEmpty() != m_trigger.isEmpty())
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
/*    s << m_cmds.count();

    for(const auto& cmd : m_cmds)
    {
        s << cmd->serialize();
    }
    */
}

void SetTrigger::deserializeImpl(QDataStream& s)
{
    int n;
    s >> m_path >> m_trigger >> m_previousTrigger;
/*
 * s >> n;
    for(;n-->0;)
    {
        QByteArray a;
        s >> a;
        auto cmd = new SetRigidity;
        cmd->deserialize(a);
        m_cmds.append(cmd);
    }*/
}
