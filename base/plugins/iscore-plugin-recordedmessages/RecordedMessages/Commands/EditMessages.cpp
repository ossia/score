#include <RecordedMessages/RecordedMessagesProcessModel.hpp>
#include <algorithm>

#include "EditMessages.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
namespace RecordedMessages
{
EditMessages::EditMessages(
    Path<ProcessModel>&& model,
    const RecordedMessagesList& lst):
  m_model{std::move(model)},
  m_new{lst}
{
    m_old = m_model.find().messages();
}

void EditMessages::undo() const
{
    m_model.find().setMessages(m_old);
}

void EditMessages::redo() const
{
    m_model.find().setMessages(m_new);
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
