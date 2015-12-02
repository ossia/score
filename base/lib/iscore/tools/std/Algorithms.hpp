#pragma once
#include <algorithm>
#include <tuple>
#include <utility>

template<typename Vector, typename Value>
auto find(Vector&& v, const Value& val)
{
    return std::find(std::begin(v), std::end(v), val);
}

template<typename Vector, typename Fun>
auto find_if(Vector&& v, Fun fun)
{
    return std::find_if(std::begin(v), std::end(v), fun);
}

template<typename Vector, typename Value>
bool contains(Vector&& v, const Value& val)
{
    return find(v, val) != v.end();
}

template<typename Vector, typename Value>
void remove_one(Vector&& v, const Value& val)
{
    auto it = find(v, val);
    if(it != v.end())
    {
        v.erase(it);
    }
}

// http://stackoverflow.com/a/26902803/1495627
template<class F, class...Ts, std::size_t...Is>
void for_each_in_tuple(const std::tuple<Ts...> & tuple, F func, std::index_sequence<Is...>){
    using expander = int[];
    (void)expander { 0, ((void)func(std::get<Is>(tuple)), 0)... };
}

template<class F, class...Ts>
void for_each_in_tuple(const std::tuple<Ts...> & tuple, F func){
    for_each_in_tuple(tuple, func, std::make_index_sequence<sizeof...(Ts)>());
}
