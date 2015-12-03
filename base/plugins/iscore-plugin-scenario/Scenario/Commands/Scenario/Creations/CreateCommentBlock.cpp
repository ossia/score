#include "CreateCommentBlock.hpp"

#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Algorithms/StandardCreationPolicy.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>

using namespace Scenario::Command;

CreateCommentBlock::CreateCommentBlock(const Path<ScenarioModel>& scenarioPath,
                                       const TimeValue& date,
                                       double yPosition):
    m_path{std::move(scenarioPath)},
    m_date{date},
    m_y{yPosition}
{
    auto& scenar = m_path.find();
    m_id = getStrongId(scenar.comments);
}

void CreateCommentBlock::undo() const
{
    auto& scenar = m_path.find();
    ScenarioCreate<CommentBlockModel>::undo(m_id, scenar);
}

void CreateCommentBlock::redo() const
{
    auto& scenar = m_path.find();
    ScenarioCreate<CommentBlockModel>::redo(m_id, m_date, m_y, scenar);
}

void CreateCommentBlock::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_date << m_y << m_id;
}

void CreateCommentBlock::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_date >> m_y >> m_id;
}
