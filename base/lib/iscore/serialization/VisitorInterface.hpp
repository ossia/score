#pragma once
#include <type_traits>
class AbstractVisitor
{
};

template<typename VisitorType>
class Visitor : public AbstractVisitor
{
};

template<typename T>
class Reader
{
};

template<typename T>
class Writer
{
};

template<typename T>
using Serializer = Visitor<Reader<T>>;

template<typename T>
using Deserializer = Visitor<Writer<T>>;

using SerializationIdentifier = int;

struct VisitorVariant
{
        AbstractVisitor& visitor;
        const SerializationIdentifier identifier;
};

template<typename DeserializerVisitor>
using enable_if_deserializer =  typename std::enable_if<std::decay<DeserializerVisitor>::type::is_visitor_tag::value>::type;
