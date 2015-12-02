#pragma once
#include <vector>
#include <iscore/serialization/DataStreamVisitor.hpp>

// TODO refactor this with objects like is_trivially_serializable<T> { ... } and enable_if...
template<typename T>
void readFrom_vector_obj_impl(
        Visitor<Reader<DataStream>>& reader,
        const std::vector<T>& vec)
{
    reader.m_stream << (int32_t)vec.size();
    for(const auto& elt : vec)
        reader.readFrom(elt);

    reader.insertDelimiter();
}

template<typename T>
void writeTo_vector_obj_impl(
        Visitor<Writer<DataStream>>& writer,
        std::vector<T>& vec)
{
    int32_t n = 0;
    writer.m_stream >> n;

    vec.clear();
    vec.resize(n);
    for(int i = 0; i < n; i++)
    {
        writer.writeTo(vec[i]);
    }

    writer.checkDelimiter();
}
