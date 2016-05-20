#include "MoveCommentBlock.hpp"

#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>


namespace Scenario
{
namespace Command
{
MoveCommentBlock::MoveCommentBlock(
        const ScenarioModel& scenar,
        Id<CommentBlockModel> id,
        TimeValue newDate,
        double newY):
    m_path{scenar},
    m_id{std::move(id)},
    m_newDate{std::move(newDate)},
    m_newY{newY}
{
    auto& cmt = scenar.comment(m_id);
    m_oldDate = cmt.date();
    m_oldY = cmt.heightPercentage();
}


void Scenario::Command::MoveCommentBlock::undo() const
{
    auto& scenar = m_path.find();
    auto& cmt = scenar.comment(m_id);
    cmt.setDate(m_oldDate);
    cmt.setHeightPercentage(m_oldY);
}

void Scenario::Command::MoveCommentBlock::redo() const
{
    auto& scenar = m_path.find();
    auto& cmt = scenar.comment(m_id);
    cmt.setDate(m_newDate);
    cmt.setHeightPercentage(m_newY);
}


void Scenario::Command::MoveCommentBlock::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_id << m_newDate << m_oldDate << m_newY << m_oldY;
}

void Scenario::Command::MoveCommentBlock::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_id >> m_newDate >> m_oldDate >> m_newY >> m_oldY;
}
}
}
