#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include "Address.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::Address& a)
{
    m_stream << a.device << a.path;
    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const iscore::Address& a)
{
    m_obj["Device"] = a.device;
    m_obj["Path"] = a.path.join("/");
}

template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::Address& a)
{
    m_stream >> a.device >> a.path;
    checkDelimiter();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(iscore::Address& a)
{
    a.device = m_obj["Device"].toString();

    a.path = m_obj["Path"].toString().split("/");
}
