#include "PropertyCommand.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>

iscore::PropertyCommand::~PropertyCommand()
{

}

void iscore::PropertyCommand::undo() const
{
    m_path.find<QObject>().setProperty(m_property.toUtf8().constData(), m_old);
}

void iscore::PropertyCommand::redo() const
{
    m_path.find<QObject>().setProperty(m_property.toUtf8().constData(), m_new);
}

void iscore::PropertyCommand::serializeImpl(QDataStream & s) const
{
    s << m_path << m_property << m_old << m_new;
}

void iscore::PropertyCommand::deserializeImpl(QDataStream & s)
{
    s >> m_path >> m_property >> m_old >> m_new;
}
