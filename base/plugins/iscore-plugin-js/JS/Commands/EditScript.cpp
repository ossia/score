#include "EditScript.hpp"

EditScript::EditScript(
    Path<JSProcess>&& model,
    const QString& text):
  iscore::SerializableCommand{
    factoryName(), commandName(), description()},
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

}

void EditScript::deserializeImpl(QDataStream& s)
{

}
