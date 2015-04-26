#pragma once

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
