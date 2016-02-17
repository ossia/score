#include <RecordedMessages/RecordedMessagesProcessModel.hpp>
#include <algorithm>

#include "EditMessages.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
namespace RecordedMessages
{
EditMessages::EditMessages(
    Path<ProcessModel>&& model,
    const QString& text):
  m_model{std::move(model)},
  m_new{text}
{
}

void EditMessages::undo() const
{
}

void EditMessages::redo() const
{

}

void EditMessages::serializeImpl(DataStreamInput& s) const
{
    s << m_model << m_old << m_new;
}

void EditMessages::deserializeImpl(DataStreamOutput& s)
{
    s >> m_model >> m_old >> m_new;
}
}
