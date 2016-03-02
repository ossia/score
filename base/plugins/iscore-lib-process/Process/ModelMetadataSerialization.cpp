#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "ModelMetadata.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T> class Reader;
template <typename T> class Writer;

template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Reader<DataStream>>::readFrom(const ColorRef& md)
{
    m_stream << md.name();
}

template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Writer<DataStream>>::writeTo(ColorRef& md)
{
    QString col_name;
    m_stream >> col_name;

    auto col = ColorRef::ColorFromString(col_name);
    if(col)
        md = *col;
}

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
/*
    QJsonArray arr;
    QColor col = md.m_color;
    arr.append(col.red());
    arr.append(col.green());
    arr.append(col.blue());
    m_obj["Color"] = arr;
*/
    m_obj["Color"] = md.m_color.name();

    m_obj["Label"] = md.m_label;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(ModelMetadata& md)
{
    md.m_scriptingName = m_obj["ScriptingName"].toString();
    md.m_comment = m_obj["Comment"].toString();

    QJsonValue color_val = m_obj["Color"];
    if(color_val.isArray())
    {
        // Old save format
        const auto& color = color_val.toArray();
        QColor col(color[0].toInt(), color[1].toInt(), color[2].toInt());
        auto sim_col = ColorRef::SimilarColor(col);
        if(sim_col)
            md.m_color = *sim_col;
    }
    else if(color_val.isString())
    {
        auto col_name = color_val.toString();
        auto col = ColorRef::ColorFromString(col_name);
        if(col)
            md.m_color = *col;
    }

    md.m_label = m_obj["Label"].toString();
}
