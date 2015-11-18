#include "AddTrigger.hpp"

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include <Scenario/Process/ScenarioModel.hpp>

using namespace Scenario::Command;

AddTrigger::AddTrigger(Path<TimeNodeModel>&& timeNodePath):
    m_path{std::move(timeNodePath)}
{

}

AddTrigger::~AddTrigger()
{
    qDeleteAll(m_cmds);
}

void AddTrigger::undo() const
{
    auto& tn = m_path.find();
    tn.trigger()->setActive(false);

    for (auto cmd : m_cmds)
    {
        cmd->undo();
        delete cmd;
    }
    m_cmds.clear();
}

void AddTrigger::redo() const
{
    auto& tn = m_path.find();
    tn.trigger()->setActive(true);

    ScenarioModel* scenar = safe_cast<ScenarioModel*>(tn.parentScenario());

    for (auto& cstrId : scenar->constraintsBeforeTimeNode(tn.id()))
    {
        auto cmd = new SetRigidity(scenar->constraints.at(cstrId), false);
        cmd->redo();
        m_cmds.push_back(cmd);
    }
}

void AddTrigger::serializeImpl(QDataStream& s) const
{
    s << m_path;
    s << m_cmds.count();

    for(const auto& cmd : m_cmds)
    {
        s << cmd->serialize();
    }
}

void AddTrigger::deserializeImpl(QDataStream& s)
{
    int n;
    s >> m_path;
    s >> n;
        for(;n-->0;)
        {
            QByteArray a;
            s >> a;
            auto cmd = new SetRigidity;
            cmd->deserialize(a);
            m_cmds.append(cmd);
        }
}

