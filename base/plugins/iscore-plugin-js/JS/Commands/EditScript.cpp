#include "EditScript.hpp"
#include <JS/JSProcessModel.hpp>

EditScript::EditScript(
    Path<JSProcessModel>&& model,
    const QString& text):
  m_model{std::move(model)},
  m_new{text}
{
	m_old = m_model.find().script();
}

void EditScript::undo() const
{
    m_model.find().setScript(m_old);
}

void EditScript::redo() const
{
    m_model.find().setScript(m_new);

}

void EditScript::serializeImpl(QDataStream& s) const
{
    s << m_model << m_old << m_new;
}

void EditScript::deserializeImpl(QDataStream& s)
{
    s >> m_model >> m_old >> m_new;
}
