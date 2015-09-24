#include "RemoveTrigger.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/Trigger/TriggerModel.hpp"

using namespace Scenario::Command;

RemoveTrigger::RemoveTrigger(Path<TimeNodeModel>&& timeNodePath):
    iscore::SerializableCommand{
	factoryName(), commandName(), description()},
    m_path{std::move(timeNodePath)}
{

}

void RemoveTrigger::undo()
{
    auto& tn = m_path.find();
    tn.trigger()->setActive(true);
}

void RemoveTrigger::redo()
{
    auto& tn = m_path.find();
    tn.trigger()->setActive(false);
}

void RemoveTrigger::serializeImpl(QDataStream& s) const
{
    s << m_path;
}

void RemoveTrigger::deserializeImpl(QDataStream& s)
{
    s >> m_path;
}

