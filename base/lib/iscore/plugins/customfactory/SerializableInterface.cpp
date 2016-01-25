#include "SerializableInterface.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<DataStream>>::readFrom(
        const boost::uuids::uuid& obj)
{
    m_stream << boost::uuids::to_string(obj);
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<DataStream>>::writeTo(
        boost::uuids::uuid& obj)
{
    std::string s;
    m_stream >> s;
    obj = boost::uuids::string_generator{}(s);
}


template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<JSONValue>>::readFrom(
        const boost::uuids::uuid& obj)
{
    val = QString::fromStdString(boost::uuids::to_string(obj));
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<JSONValue>>::writeTo(
        boost::uuids::uuid& obj)
{
    obj = boost::uuids::string_generator{}(val.toString().toStdString());
}



namespace iscore
{
SerializableInterface::SerializableInterface()
{

}

SerializableInterface::~SerializableInterface()
{

}

uuid_t SerializableInterface::uuid() const
{
    return boost::uuids::nil_uuid();

}

void SerializableInterface::serialize(const VisitorVariant &vis) const
{
    switch(vis.identifier)
    {
        case DataStream::type():
        {
            auto& v = static_cast<DataStream::Serializer&>(vis.visitor);
            v.readFrom(uuid());
            break;
        }
        case JSONObject::type():
        {
            auto& v = static_cast<JSONObject::Serializer&>(vis.visitor);
            v.m_obj["uuid"] = toJsonValue(uuid());
            break;
        }
        default:
            ISCORE_ABORT;
    }

    serialize_impl(vis);
}

void SerializableInterface::serialize_impl(const VisitorVariant &vis) const
{

}
}



