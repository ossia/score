#include <iscore/serialization/DataStreamVisitor.hpp>

#include "PropertyCommand.hpp"
#include <iscore/tools/ObjectPath.hpp>

iscore::PropertyCommand::~PropertyCommand() = default;

void iscore::PropertyCommand::undo() const
{
    m_path.find<QObject>().setProperty(m_property.toUtf8().constData(), m_old);
}

void iscore::PropertyCommand::redo() const
{
    m_path.find<QObject>().setProperty(m_property.toUtf8().constData(), m_new);
}

void iscore::PropertyCommand::serializeImpl(DataStreamInput & s) const
{
    s << m_path << m_property << m_old << m_new;
}

void iscore::PropertyCommand::deserializeImpl(DataStreamOutput & s)
{
    s >> m_path >> m_property >> m_old >> m_new;
}
