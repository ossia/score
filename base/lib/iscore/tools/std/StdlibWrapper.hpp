#pragma once
#include <string>
#include <memory>
#include <QDataStream>
#include <QDebug>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <boost/iterator/indirect_iterator.hpp>

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

template<template<class, class> class Container,
         typename T,
         typename U = std::allocator<T*>>
class IndirectContainer : Container<T*, U>
{
    public:
        using ctnr_t = Container<T*, U>;
        using ctnr_t::ctnr_t;

        auto begin()
        { return boost::make_indirect_iterator(ctnr_t::begin()); }
        auto end()
        { return boost::make_indirect_iterator(ctnr_t::end()); }
        auto begin() const
        { return boost::make_indirect_iterator(ctnr_t::begin()); }
        auto end() const
        { return boost::make_indirect_iterator(ctnr_t::end()); }
        auto cbegin() const
        { return boost::make_indirect_iterator(ctnr_t::cbegin()); }
        auto cend() const
        { return boost::make_indirect_iterator(ctnr_t::cend()); }
};


template<template<class, class> class Container,
         typename T,
         typename U = std::allocator<std::unique_ptr<T>>>
class PtrContainer : Container<std::unique_ptr<T>, U>
{
    public:
        using ctnr_t = Container<std::unique_ptr<T>, U>;
        using ctnr_t::ctnr_t;
        using ctnr_t::emplace_back;

        auto begin()
        { return boost::make_indirect_iterator(ctnr_t::begin()); }
        auto end()
        { return boost::make_indirect_iterator(ctnr_t::end()); }
        auto begin() const
        { return boost::make_indirect_iterator(ctnr_t::begin()); }
        auto end() const
        { return boost::make_indirect_iterator(ctnr_t::end()); }
        auto cbegin() const
        { return boost::make_indirect_iterator(ctnr_t::cbegin()); }
        auto cend() const
        { return boost::make_indirect_iterator(ctnr_t::cend()); }
};

template<typename T>
using OwningVector = PtrContainer<std::vector, T>;

// http://stackoverflow.com/a/21174979/1495627
template<typename Derived, typename Base, typename Del>
std::unique_ptr<Derived>
static_unique_ptr_cast( std::unique_ptr<Base, Del>&& p )
{
    return std::unique_ptr<Derived>(static_cast<Derived *>(p.release()));
}

template<typename Derived, typename Base, typename Del>
std::unique_ptr<Derived>
dynamic_unique_ptr_cast( std::unique_ptr<Base, Del>&& p )
{
    if(Derived *result = dynamic_cast<Derived *>(p.get())) {
        p.release();
        return std::unique_ptr<Derived>(result);
    }
    return nullptr;
}

#ifdef ISCORE_DEBUG
template<typename T,
         typename U>
auto safe_unique_ptr_cast(std::unique_ptr<U> other)
{
    auto res = dynamic_unique_ptr_cast<T>(other);
    ISCORE_ASSERT(res);
    return res;
}

#else
#define safe_unique_ptr_cast static_unique_ptr_cast
#endif


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

