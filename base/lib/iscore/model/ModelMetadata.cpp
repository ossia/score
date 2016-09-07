#include "ModelMetadata.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>

namespace iscore
{
ModelMetadata::ModelMetadata(const ModelMetadata &other) :
    QObject {}
{
    setName(other.getName());
    setComment(other.getComment());
    setColor(other.getColor());
    setLabel(other.getLabel());
    setExtendedMetadata(other.getExtendedMetadata());
}

ModelMetadata &iscore::ModelMetadata::operator=(const ModelMetadata &other)
{
    setName(other.getName());
    setComment(other.getComment());
    setColor(other.getColor());
    setLabel(other.getLabel());
    setExtendedMetadata(other.getExtendedMetadata());

    return *this;
}


const QString& ModelMetadata::getName() const
{
    return m_scriptingName;
}

const QString& ModelMetadata::getComment() const
{
    return m_comment;
}

ColorRef ModelMetadata::getColor() const
{
    return m_color;
}

const QString& ModelMetadata::getLabel() const
{
    return m_label;
}

const QVariantMap& ModelMetadata::getExtendedMetadata() const
{
    return m_extendedMetadata;
}


void ModelMetadata::setName(const QString& arg)
{
    if(m_scriptingName == arg)
    {
        return;
    }

    m_scriptingName = arg;
    emit NameChanged(arg);
    emit metadataChanged();
}

void ModelMetadata::setComment(const QString& arg)
{
    if(m_comment == arg)
    {
        return;
    }

    m_comment = arg;
    emit CommentChanged(arg);
    emit metadataChanged();
}

void ModelMetadata::setColor(ColorRef arg)
{
    if(m_color == arg)
    {
        return;
    }

    m_color = arg;
    emit ColorChanged(arg);
    emit metadataChanged();
}

void ModelMetadata::setLabel(const QString& arg)
{
    if(m_label == arg)
    {
        return;
    }

    m_label = arg;
    emit LabelChanged(arg);
    emit metadataChanged();
}

void ModelMetadata::setExtendedMetadata(const QVariantMap& arg)
{
    if(m_extendedMetadata == arg)
    {
        return;
    }

    m_extendedMetadata = arg;
    emit ExtendedMetadataChanged(arg);
    emit metadataChanged();
}
}

// MOVEME
template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const iscore::ColorRef& md)
{
    m_stream << md.name();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<DataStream>>::writeTo(iscore::ColorRef& md)
{
    QString col_name;
    m_stream >> col_name;

    auto col = iscore::ColorRef::ColorFromString(col_name);
    if(col)
        md = *col;
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const iscore::ModelMetadata& md)
{
    m_stream << md.m_scriptingName
             << md.m_comment
             << md.m_color
             << md.m_label
             << md.m_extendedMetadata;

    insertDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<DataStream>>::writeTo(iscore::ModelMetadata& md)
{
    m_stream >> md.m_scriptingName
             >> md.m_comment
             >> md.m_color
             >> md.m_label
             >> md.m_extendedMetadata;

    checkDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const iscore::ModelMetadata& md)
{
    m_obj[strings.ScriptingName] = md.m_scriptingName;
    m_obj[strings.Comment] = md.m_comment;
    m_obj[strings.Color] = md.m_color.name();
    m_obj[strings.Label] = md.m_label;
    if(!md.m_extendedMetadata.empty())
    {
        m_obj.insert(strings.Extended, QJsonObject::fromVariantMap(md.m_extendedMetadata));
    }
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(iscore::ModelMetadata& md)
{
    md.m_scriptingName = m_obj[strings.ScriptingName].toString();
    md.m_comment = m_obj[strings.Comment].toString();

    QJsonValue color_val = m_obj[strings.Color];
    if(color_val.isArray())
    {
        // Old save format
        const auto& color = color_val.toArray();
        QColor col(color[0].toInt(), color[1].toInt(), color[2].toInt());
        auto sim_col = iscore::ColorRef::SimilarColor(col);
        if(sim_col)
            md.m_color = *sim_col;
    }
    else if(color_val.isString())
    {
        auto col_name = color_val.toString();
        auto col = iscore::ColorRef::ColorFromString(col_name);
        if(col)
            md.m_color = *col;
    }

    md.m_label = m_obj[strings.Label].toString();

    {
        auto it = m_obj.find(strings.Extended);
        if(it != m_obj.end())
        {
            md.m_extendedMetadata = it->toObject().toVariantMap();
        }
    }
}
