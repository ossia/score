#include "EditScript.hpp"
#include <Loop/LoopProcessModel.hpp>

EditScript::EditScript(
    Path<LoopProcessModel>&& model,
    const QString& text):
  m_model{std::move(model)},
  m_new{text}
{
}

void EditScript::undo() const
{
}

void EditScript::redo() const
{

}

void EditScript::serializeImpl(QDataStream& s) const
{
    s << m_model << m_old << m_new;
}

void EditScript::deserializeImpl(QDataStream& s)
{
    s >> m_model >> m_old >> m_new;
}
