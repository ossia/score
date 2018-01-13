// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <JS/JSProcessModel.hpp>
#include <algorithm>

#include "EditScript.hpp"
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/path/PathSerialization.hpp>
namespace JS
{
EditScript::EditScript(const ProcessModel& model, const QString& text)
    : m_model{model}, m_new{text}
{
  m_old = model.script();
}

void EditScript::undo(const score::DocumentContext& ctx) const
{
  m_model.find(ctx).setScript(m_old);
}

void EditScript::redo(const score::DocumentContext& ctx) const
{
  m_model.find(ctx).setScript(m_new);
}

void EditScript::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_old << m_new;
}

void EditScript::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_old >> m_new;
}
}
