#pragma once
#include <vector>
#include <iscore/serialization/DataStreamVisitor.hpp>

// TODO refactor this with objects like is_trivially_serializable<T> { ... } and enable_if...

template<typename... Args>
struct TSerializer<DataStream, std::vector<Args...>>
{
        static void readFrom(
                DataStream::Serializer& s,
                const std::vector<Args...>& vec)
        {
            s.stream() << (int32_t)vec.size();
            for(const auto& elt : vec)
                s.readFrom(elt);

            s.insertDelimiter();
        }

        static void writeTo(
                DataStream::Deserializer& s,
                std::vector<Args...>& vec)
        {
            int32_t n = 0;
            s.stream() >> n;

            vec.clear();
            vec.resize(n);
            for(int i = 0; i < n; i++)
            {
                s.writeTo(vec[i]);
            }

            s.checkDelimiter();
        }

};
