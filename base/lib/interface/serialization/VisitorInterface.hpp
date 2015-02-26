#pragma once

template<typename VisitorType>
class Visitor
{
    public:
        //template<typename T>
        //void visit(T&);
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
