#pragma once
#include <boost/iterator/indirect_iterator.hpp>
#include <memory>
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

#include <array>
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
