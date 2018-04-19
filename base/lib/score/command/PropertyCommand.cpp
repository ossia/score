// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PropertyCommand.hpp"

#include <score/model/path/ObjectPath.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

score::PropertyCommand::~PropertyCommand() = default;

void score::PropertyCommand::undo(const score::DocumentContext& ctx) const
{
  m_path.find<QObject>(ctx).setProperty(
      m_property.toUtf8().constData(), m_old);
}

void score::PropertyCommand::redo(const score::DocumentContext& ctx) const
{
  m_path.find<QObject>(ctx).setProperty(
      m_property.toUtf8().constData(), m_new);
}

void score::PropertyCommand::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_property << m_old << m_new;
}

void score::PropertyCommand::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_property >> m_old >> m_new;
}
