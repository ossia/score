#pragma once
#include <string>
#include <QDataStream>
#include <QDebug>
#include <iscore/serialization/DataStreamVisitor.hpp>

// TODO refactor this with objects like is_trivially_serializable<T> { ... } and enable_if...
template<typename T>
void readFrom_vector_obj_impl(
        Visitor<Reader<DataStream>>& reader,
        const std::vector<T>& vec)
{
    reader.m_stream << (int)vec.size();
    for(const auto& elt : vec)
        reader.readFrom(elt);

    reader.insertDelimiter();
}

template<typename T>
void writeTo_vector_obj_impl(
        Visitor<Writer<DataStream>>& writer,
        std::vector<T>& vec)
{
    int n = 0;
    writer.m_stream >> n;

    vec.clear();
    vec.resize(n);
    for(int i = 0; i < n; i++)
    {
        writer.writeTo(vec[i]);
    }

    writer.checkDelimiter();
}

inline QDataStream& operator<< (QDataStream& stream, const std::string& obj)
{
    uint32_t size = obj.size();
    stream << size;

    stream.writeRawData(obj.data(), size);
    return stream;
}

inline QDataStream& operator>> (QDataStream& stream, std::string& obj)
{
    uint32_t n = 0;
    stream >> n;
    obj.resize(n);

    char* addr = n > 0 ? &obj[0] : nullptr;
    stream.readRawData(addr, n);

    return stream;
}

inline QDebug operator<< (QDebug debug, const std::string& obj)
{
    debug << obj.c_str();
    return debug;
}
