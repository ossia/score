#pragma once
#include <boost/iterator/indirect_iterator.hpp>
#include <memory>
#include <array>

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

template<class Container>
class IndirectContainerWrapper
{
    public:
        Container& container;

        auto begin()
        { return boost::make_indirect_iterator(container.begin()); }
        auto end()
        { return boost::make_indirect_iterator(container.end()); }
        auto begin() const
        { return boost::make_indirect_iterator(container.begin()); }
        auto end() const
        { return boost::make_indirect_iterator(container.end()); }
        auto cbegin() const
        { return boost::make_indirect_iterator(container.cbegin()); }
        auto cend() const
        { return boost::make_indirect_iterator(container.cend()); }
};

template<typename T>
auto wrap_indirect(T& container)
{
    return IndirectContainerWrapper<T>{container};
}

template<typename T,
         int N>
class IndirectArray
{
        std::array<T*, N> array;
    public:
        using value_type = T;
        template<typename... Args>
        IndirectArray(Args&&... args):
            array{{std::forward<Args>(args)...}}
        {

        }

        auto begin()
        { return boost::make_indirect_iterator(array.begin()); }
        auto end()
        { return boost::make_indirect_iterator(array.end()); }
        auto begin() const
        { return boost::make_indirect_iterator(array.begin()); }
        auto end() const
        { return boost::make_indirect_iterator(array.end()); }
        auto cbegin() const
        { return boost::make_indirect_iterator(array.cbegin()); }
        auto cend() const
        { return boost::make_indirect_iterator(array.cend()); }

        auto& operator[](int pos)
        { return *array[pos]; }
        auto& operator[](int pos) const
        { return *array[pos]; }
};

template<typename Map_T>
class IndirectMap
{
    public:
        auto begin()        { return boost::make_indirect_iterator(map.begin()); }
        auto begin() const  { return boost::make_indirect_iterator(map.begin()); }

        auto cbegin()       { return boost::make_indirect_iterator(map.cbegin()); }
        auto cbegin() const { return boost::make_indirect_iterator(map.cbegin()); }

        auto end()          { return boost::make_indirect_iterator(map.end()); }
        auto end() const    { return boost::make_indirect_iterator(map.end()); }

        auto cend()         { return boost::make_indirect_iterator(map.cend()); }
        auto cend() const   { return boost::make_indirect_iterator(map.cend()); }

        auto empty() const { return map.empty(); }

        template<typename K>
        auto find(K&& key)
        {
            return map.find(std::forward<K>(key));
        }

        template<typename E>
        auto insert(E&& elt)
        {
            return map.insert(std::forward<E>(elt));
        }

    protected:
        Map_T map;
};

