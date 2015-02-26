#include "ModelMetadata.hpp"


//// Simple properties
QString ModelMetadata::name() const
{
    return m_scriptingName;
}

QString ModelMetadata::comment() const
{
    return m_comment;
}

QColor ModelMetadata::color() const
{
    return m_color;
}

QString ModelMetadata::label() const
{
    return m_label;
}




void ModelMetadata::setName(QString arg)
{
    if(m_scriptingName == arg)
    {
        return;
    }

    m_scriptingName = arg;
    emit nameChanged(arg);
    emit metadataChanged();
}

void ModelMetadata::setComment(QString arg)
{
    if(m_comment == arg)
    {
        return;
    }

    m_comment = arg;
    emit commentChanged(arg);
    emit metadataChanged();
}

void ModelMetadata::setColor(QColor arg)
{
    if(m_color == arg)
    {
        return;
    }

    m_color = arg;
    emit colorChanged(arg);
    emit metadataChanged();
}

void ModelMetadata::setLabel(QString arg)
{
    if(m_label == arg)
    {
        return;
    }

    m_label = arg;
    emit labelChanged(arg);
    emit metadataChanged();
}
