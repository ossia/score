#include "ModelMetadata.hpp"


//// Simple properties
const QString& ModelMetadata::name() const
{
    return m_scriptingName;
}

const QString& ModelMetadata::comment() const
{
    return m_comment;
}

const QColor& ModelMetadata::color() const
{
    return m_color;
}

const QString& ModelMetadata::label() const
{
    return m_label;
}




void ModelMetadata::setName(const QString& arg)
{
    if(m_scriptingName == arg)
    {
        return;
    }

    m_scriptingName = arg;
    emit nameChanged(arg);
    emit metadataChanged();
}

void ModelMetadata::setComment(const QString& arg)
{
    if(m_comment == arg)
    {
        return;
    }

    m_comment = arg;
    emit commentChanged(arg);
    emit metadataChanged();
}

void ModelMetadata::setColor(const QColor& arg)
{
    if(m_color == arg)
    {
        return;
    }

    m_color = arg;
    emit colorChanged(arg);
    emit metadataChanged();
}

void ModelMetadata::setLabel(const QString& arg)
{
    if(m_label == arg)
    {
        return;
    }

    m_label = arg;
    emit labelChanged(arg);
    emit metadataChanged();
}
