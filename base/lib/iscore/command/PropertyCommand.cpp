// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <iscore/serialization/DataStreamVisitor.hpp>

#include "PropertyCommand.hpp"
#include <iscore/model/path/ObjectPath.hpp>

iscore::PropertyCommand::~PropertyCommand() = default;

void iscore::PropertyCommand::undo(const iscore::DocumentContext& ctx) const
{
  m_path.find<QObject>(ctx).setProperty(m_property.toUtf8().constData(), m_old);
}

void iscore::PropertyCommand::redo(const iscore::DocumentContext& ctx) const
{
  m_path.find<QObject>(ctx).setProperty(m_property.toUtf8().constData(), m_new);
}

void iscore::PropertyCommand::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_property << m_old << m_new;
}

void iscore::PropertyCommand::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_property >> m_old >> m_new;
}
