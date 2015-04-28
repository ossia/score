#include "ModelMetadata.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const ModelMetadata& md)
{
    m_stream << md.m_scriptingName
             << md.m_comment
             << md.m_color
             << md.m_label;

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ModelMetadata& md)
{
    m_stream >> md.m_scriptingName
             >> md.m_comment
             >> md.m_color
             >> md.m_label;

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const ModelMetadata& md)
{
    m_obj["ScriptingName"] = md.m_scriptingName;
    m_obj["Comment"] = md.m_comment;

    m_obj["Color"] = QJsonArray {md.m_color.red(), md.m_color.green(), md.m_color.blue()};

    m_obj["Label"] = md.m_label;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(ModelMetadata& md)
{
    md.m_scriptingName = m_obj["ScriptingName"].toString();
    md.m_comment = m_obj["Comment"].toString();

    QJsonArray color = m_obj["Color"].toArray();
    md.m_color = QColor(color[0].toInt(), color[1].toInt(), color[2].toInt());

    md.m_label = m_obj["Label"].toString();
}
