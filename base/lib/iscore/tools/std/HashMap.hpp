#pragma once
#include <ossia/detail/config.hpp>
#include <iscore/serialization/IsTemplate.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <hopscotch_map.h>
namespace iscore
{
template<typename... Args>
using hash_map = tsl::hopscotch_map<Args...>;

template<typename Map>
void optimize_hash_map(Map& map)
{
  map.max_load_factor(0.1);
  map.reserve(map.size());
}
}

template <typename T, typename U>
struct is_template<iscore::hash_map<T, U>> : std::true_type
{
};

template<typename T, typename U>
struct TSerializer<DataStream, void, iscore::hash_map<T, U>>
{
        static void readFrom(
                DataStream::Serializer& s,
                const iscore::hash_map<T, U>& obj)
        {
            auto& st = s.stream();
            st << (int32_t) obj.size();
            for(const auto& e : obj)
            {
                st << e.first << e.second;
            }
        }

        static void writeTo(
                DataStream::Deserializer& s,
                iscore::hash_map<T, U>& obj)
        {
            auto& st = s.stream();
            int32_t n;
            st >> n;
            for(int i = 0; i < n; i++)
            {
                T key;
                U value;
                st >> key >> value;
                obj.emplace(std::move(key), std::move(value));
            }
        }
};
