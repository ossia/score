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
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const State::Address& a)
{
    m_stream << a.device << a.path;
    insertDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const State::Address& a)
{
    m_obj[strings.Device] = a.device;
    m_obj[strings.Path] = a.path.join('/');
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<DataStream>>::writeTo(State::Address& a)
{
    m_stream >> a.device >> a.path;
    checkDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(State::Address& a)
{
    a.device = m_obj[strings.Device].toString();

    auto path = m_obj[strings.Path].toString();

    if(!path.isEmpty())
        a.path = m_obj[strings.Path].toString().split('/');
}
