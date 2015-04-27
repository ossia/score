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

    QJsonObject color;
    color["R"] = md.m_color.red();
    color["G"] = md.m_color.green();
    color["B"] = md.m_color.blue();
    m_obj["Color"] = color;

    m_obj["Label"] = md.m_label;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(ModelMetadata& md)
{
    md.m_scriptingName = m_obj["ScriptingName"].toString();
    md.m_comment = m_obj["Comment"].toString();

    QJsonObject color = m_obj["Color"].toObject();
    md.m_color = QColor(color["R"].toInt(), color["G"].toInt(), color["B"].toInt());

    md.m_label = m_obj["Label"].toString();
}
