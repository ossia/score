#include "AddTrigger.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/Trigger/TriggerModel.hpp"

using namespace Scenario::Command;

AddTrigger::AddTrigger(Path<TimeNodeModel>&& timeNodePath):
    iscore::SerializableCommand{
	factoryName(), commandName(), description()},
    m_path{std::move(timeNodePath)}
{

}

void AddTrigger::undo()
{
    auto& tn = m_path.find();
    tn.trigger()->setActive(false);
}

void AddTrigger::redo()
{
    auto& tn = m_path.find();
    tn.trigger()->setActive(true);
}

void AddTrigger::serializeImpl(QDataStream& s) const
{
    s << m_path;
}

void AddTrigger::deserializeImpl(QDataStream& s)
{
    s >> m_path;
}

