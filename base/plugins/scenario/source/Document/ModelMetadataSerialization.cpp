#include "ModelMetadata.hpp"

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

// TODO put them in their own folder.
QDataStream& operator<< (QDataStream& s, const ModelMetadata& m)
{
    s << m.name() << m.comment() << m.color() << m.label();

    return s;
}

QDataStream& operator>> (QDataStream& s, ModelMetadata& m)
{
    QString name, comment, label;
    QColor color;
    s >> name >> comment >> color >> label;

    m.setName(name);
    m.setComment(comment);
    m.setColor(color);
    m.setLabel(label);

    return s;
}





template<>
void Visitor<Reader<DataStream>>::readFrom(const ModelMetadata& md)
{
    m_stream << md.m_scriptingName
             << md.m_comment
             << md.m_color
             << md.m_label
             << md.m_pluginsMetadata;

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ModelMetadata& md)
{
    m_stream >> md.m_scriptingName
             >> md.m_comment
             >> md.m_color
             >> md.m_label
             >> md.m_pluginsMetadata;

    checkDelimiter();
}

template<>
void Visitor<Reader<JSON>>::readFrom(const ModelMetadata& md)
{
    m_obj["ScriptingName"] = md.m_scriptingName;
    m_obj["Comment"] = md.m_comment;

    QJsonObject color;
    color["R"] = md.m_color.red();
    color["G"] = md.m_color.green();
    color["B"] = md.m_color.blue();
    m_obj["Color"] = color;

    m_obj["Label"] = md.m_label;
    m_obj["PluginsMetadata"] = toJsonArray(md.m_pluginsMetadata);
}

template<>
void Visitor<Writer<JSON>>::writeTo(ModelMetadata& md)
{
    md.m_scriptingName = m_obj["ScriptingName"].toString();
    md.m_comment = m_obj["Comment"].toString();

    QJsonObject color = m_obj["Color"].toObject();
    md.m_color = QColor(color["R"].toInt(), color["G"].toInt(), color["B"].toInt());

    md.m_label = m_obj["Label"].toString();
    fromJsonArray(m_obj["PluginsMetadata"].toArray(), md.m_pluginsMetadata);
}
