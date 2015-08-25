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

    ISCORE_ABORT;
}

template<typename TheClass>
TheClass& deserialize_dyn(const VisitorVariant& vis, TheClass& s)
{
    switch(vis.identifier)
    {
        case DataStream::type():
        {
            static_cast<DataStream::Deserializer&>(vis.visitor).writeTo(s);
            break;
        }
        case JSONObject::type():
        {
            static_cast<JSONObject::Deserializer&>(vis.visitor).writeTo(s);
            break;
        }
        default:
            ISCORE_ABORT;
    }

    return s;
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
            break;
        }
        case JSONObject::type():
        {
            static_cast<JSONObject::Deserializer&>(vis.visitor).writeTo(s);
            break;
        }
        default:
            ISCORE_ABORT;
    }
    return s;
}

template<typename Functor>
auto deserialize_dyn(const VisitorVariant& vis, Functor&& fun)
{
    switch(vis.identifier)
    {
        case DataStream::type():
        {
            return fun(static_cast<DataStream::Deserializer&>(vis.visitor));
            break;
        }
        case JSONObject::type():
        {
            return fun(static_cast<JSONObject::Deserializer&>(vis.visitor));
            break;
        }
        default:
            ISCORE_ABORT;
            throw;
    }
}
