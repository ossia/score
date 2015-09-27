#include "RemoveTrigger.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/Trigger/TriggerModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include "Process/ScenarioModel.hpp"

using namespace Scenario::Command;

RemoveTrigger::RemoveTrigger(Path<TimeNodeModel>&& timeNodePath):
    iscore::SerializableCommand{
    factoryName(), commandName(), description()},
    m_path{std::move(timeNodePath)}
{

}

RemoveTrigger::~RemoveTrigger()
{
    qDeleteAll(m_cmds);
}

void RemoveTrigger::undo()
{
    auto& tn = m_path.find();
    tn.trigger()->setActive(true);

    for (auto cmd : m_cmds)
    {
        cmd->undo();
        delete cmd;
        m_cmds.removeAll(cmd);
    }
}

void RemoveTrigger::redo()
{
    auto& tn = m_path.find();
    tn.trigger()->setActive(false);

    ScenarioModel* scenar = safe_cast<ScenarioModel*>(tn.parentScenario());

    for (auto& cstrId : scenar->constraintsBeforeTimeNode(tn.id()))
    {
        auto cmd = new SetRigidity(iscore::IDocument::path(scenar->constraints.at(cstrId)), true);
        cmd->redo();
        m_cmds.push_back(cmd);
    }
}

void RemoveTrigger::serializeImpl(QDataStream& s) const
{
    s << m_path;
    s << m_cmds.count();

    for(const auto& cmd : m_cmds)
    {
        s << cmd->serialize();
    }
}

void RemoveTrigger::deserializeImpl(QDataStream& s)
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
