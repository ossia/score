// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MoveCommentBlock.hpp"

#include <Process/TimeValueSerialization.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
MoveCommentBlock::MoveCommentBlock(
    const ProcessModel& scenar,
    Id<CommentBlockModel>
        id,
    TimeVal newDate,
    double newY)
    : m_path{scenar}
    , m_id{std::move(id)}
    , m_newDate{std::move(newDate)}
    , m_newY{newY}
{
  auto& cmt = scenar.comment(m_id);
  m_oldDate = cmt.date();
  m_oldY = cmt.heightPercentage();
}

void Scenario::Command::MoveCommentBlock::undo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);
  auto& cmt = scenar.comment(m_id);
  cmt.setDate(m_oldDate);
  cmt.setHeightPercentage(m_oldY);
}

void Scenario::Command::MoveCommentBlock::redo(const score::DocumentContext& ctx) const
{
  auto& scenar = m_path.find(ctx);
  auto& cmt = scenar.comment(m_id);
  cmt.setDate(m_newDate);
  cmt.setHeightPercentage(m_newY);
}

void Scenario::Command::MoveCommentBlock::serializeImpl(
    DataStreamInput& s) const
{
  s << m_path << m_id << m_newDate << m_oldDate << m_newY << m_oldY;
}

void Scenario::Command::MoveCommentBlock::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_id >> m_newDate >> m_oldDate >> m_newY >> m_oldY;
}
}
}
