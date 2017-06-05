#include "SetCommentText.hpp"

#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
SetCommentText::SetCommentText(
    const CommentBlockModel& model, QString newComment)
    : m_path{model}
    , m_newComment{std::move(newComment)}
    , m_oldComment{model.content()}
{

}

void SetCommentText::undo() const
{
  auto& cmt = m_path.find();
  cmt.setContent(m_oldComment);
}

void SetCommentText::redo() const
{
  auto& cmt = m_path.find();
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
