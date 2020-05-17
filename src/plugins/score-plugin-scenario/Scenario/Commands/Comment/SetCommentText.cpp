// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SetCommentText.hpp"

#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Scenario
{
namespace Command
{
SetCommentText::SetCommentText(const CommentBlockModel& model, QString newComment)
    : m_path{model}, m_newComment{std::move(newComment)}, m_oldComment{model.content()}
{
}

void SetCommentText::undo(const score::DocumentContext& ctx) const
{
  auto& cmt = m_path.find(ctx);
  cmt.setContent(m_oldComment);
}

void SetCommentText::redo(const score::DocumentContext& ctx) const
{
  auto& cmt = m_path.find(ctx);
  cmt.setContent(m_newComment);
}

void SetCommentText::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_newComment << m_oldComment;
}

void SetCommentText::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_newComment >> m_oldComment;
}
}
}
