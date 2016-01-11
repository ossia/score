#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <algorithm>

#include "SetTrigger.hpp"
#include <State/Expression.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/TreeNode.hpp>

using namespace iscore;

namespace Scenario
{
namespace Command
{

SetTrigger::SetTrigger(Path<TimeNodeModel>&& timeNodePath,
                       State::Trigger trigger) :
m_path {std::move(timeNodePath) },
m_trigger(std::move(trigger))
{
    auto& tn = m_path.find();
    m_previousTrigger = tn.trigger()->expression();
}

SetTrigger::~SetTrigger()
{

}

void SetTrigger::undo() const
{
    auto& tn = m_path.find();
    tn.trigger()->setExpression(m_previousTrigger);
}

void SetTrigger::redo() const
{
    auto& tn = m_path.find();
    tn.trigger()->setExpression(m_trigger);
}

void SetTrigger::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_trigger << m_previousTrigger;
}

void SetTrigger::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_trigger >> m_previousTrigger;
}

}
}
