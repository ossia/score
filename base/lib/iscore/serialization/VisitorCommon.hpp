#pragma once

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<typename TheClass>
void serialize_dyn(const VisitorVariant& vis, const TheClass& s)
{
    if(vis.identifier == DataStream::type())
    {
        static_cast<DataStream::Serializer&>(vis.visitor).readFrom(s);
        return;
    }
    else if(vis.identifier == JSONObject::type())
    {
        static_cast<JSONObject::Serializer&>(vis.visitor).readFrom(s);
        return;
    }

    Q_ASSERT(false);
}

template<typename TheClass>
TheClass& deserialize_dyn(const VisitorVariant& vis, TheClass& s)
{
    switch(vis.identifier)
    {
        case DataStream::type():
        {
            static_cast<DataStream::Deserializer&>(vis.visitor).writeTo(s);
            return s;
        }
        case JSONObject::type():
        {
            static_cast<JSONObject::Deserializer&>(vis.visitor).writeTo(s);
            return s;
        }
        default:
            Q_ASSERT(false);
    }
}
template<typename TheClass>
TheClass deserialize_dyn(const VisitorVariant& vis)
{
    TheClass s;

    switch(vis.identifier)
    {
        case DataStream::type():
        {
            static_cast<DataStream::Deserializer&>(vis.visitor).writeTo(s);
            return s;
        }
        case JSONObject::type():
        {
            static_cast<JSONObject::Deserializer&>(vis.visitor).writeTo(s);
            return s;
        }
        default:
            Q_ASSERT(false);
    }
}
