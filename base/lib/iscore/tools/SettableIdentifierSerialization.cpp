#include <boost/optional/optional.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/tools/std/Optional.hpp>
#include <sys/types.h>

template <typename T> class Reader;
template <typename T> class Writer;


// TODO should not be used. Save as optional json value instead.
template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const boost::optional<int32_t>& obj)
{
    if(obj)
    {
        m_obj["id"] = get(obj);
    }
    else
    {
        m_obj["id"] = "none";
    }
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(boost::optional<int32_t>& obj)
{
    if(m_obj["id"].toString() == "none")
    {
        reset(obj);
    }
    else
    {
        obj = m_obj["id"].toInt();
    }
}
