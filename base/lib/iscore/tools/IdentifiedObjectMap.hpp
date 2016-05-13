#pragma once
#include <iscore/tools/IdentifiedObject.hpp>

#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>

#include <boost/iterator/indirect_iterator.hpp>
// This file contains a fast map for items based on their identifier,
// based on boost's multi-index maps.

namespace bmi = boost::multi_index;

template<class Element, class Model = Element, class Enable = void>
class IdContainer
{

};

template<typename Element, typename Model, typename Map>
/**
 * @brief The MapBase class
 *
 * A generic map type, which provides reference-like access to the stored pointers.
 */
class MapBase
{
    public:
        using value_type = Element;
        using model_type = Model;
        auto begin() const { return boost::make_indirect_iterator(m_map.begin()); }
        auto cbegin() const { return boost::make_indirect_iterator(m_map.cbegin()); }
        auto end() const { return boost::make_indirect_iterator(m_map.end()); }
        auto cend() const { return boost::make_indirect_iterator(m_map.cend()); }

        MapBase() = default;

        template<typename T>
        MapBase(const T& container)
        {
            for(auto& element : container)
            {
                insert(&element);
            }
        }

        void insert(value_type* t)
        {
            ISCORE_ASSERT(m_map.find(t->id()) == m_map.end());
            m_map.insert(t);
        }

        std::size_t size() const
        { return m_map.size(); }

        bool empty() const
        { return m_map.empty(); }

        template<typename T>
        void remove(const T& t)
        { m_map.erase(t); }

        void clear()
        { m_map.clear(); }

        auto find(const Id<model_type>& id) const
        { return boost::make_indirect_iterator(m_map.find(id)); }

        auto& get() { return m_map.template get<0>(); }
        const auto& get() const { return m_map.template get<0>(); }

    protected:
        Map m_map;
};

// We have to write two implementations since const_mem_fun does not handle inheritance.

// This map is for classes which directly inherit from
// IdentifiedObject<T> and don't have an id() method by themselves.
template<typename Element, typename Model>
class IdContainer<Element, Model,
        std::enable_if_t<
            std::is_base_of<
                IdentifiedObject<Model>,
                Element
            >::value
        >> : public MapBase<
            Element,
            Model,
            bmi::multi_index_container<
                Element*,
                bmi::indexed_by<
                    bmi::hashed_unique<
                        bmi::const_mem_fun<
                            IdentifiedObject<Model>,
                            const Id<Model>&,
                            &IdentifiedObject<Model>::id
                        >
                    >,
                    bmi::sequenced<>
                >
            >
        >
{
    public:
   using MapBase<
    Element,
    Model,
    bmi::multi_index_container<
        Element*,
        bmi::indexed_by<
            bmi::hashed_unique<
                bmi::const_mem_fun<
                    IdentifiedObject<Model>,
                    const Id<Model>&,
                    &IdentifiedObject<Model>::id
                >
            >,
            bmi::sequenced<>
        >
    >
>::MapBase;

        Element& at(const Id<Model>& id) const
        {
            if(id.m_ptr)
            {
                ISCORE_ASSERT(id.m_ptr->parent() == (*this->m_map.find(id))->parent());
                return safe_cast<Element&>(*id.m_ptr);
            }
            auto item = this->m_map.find(id);
            ISCORE_ASSERT(item != this->m_map.end());

            id.m_ptr = *item;
            return safe_cast<Element&>(**item);
        }

        void swap(const Id<Model>& t1, const Id<Model>& t2)
        {
            /*
            auto first = this->m_map.find(t1);
            auto first_pos_idx = this->m_map.template project<1>(first);

            auto second = this->m_map.find(t2);
            auto second_pos_idx = this->m_map.template project<1>(second);

            auto t = *first_pos_idx;
            *first_pos_idx = *second_pos_idx;
            *second_pos_idx = t;
            */
        }


};


// This map is for classes which directly have an id() method
// like a Presenter whose id() would return its model's.
template<typename Element, typename Model>
class IdContainer<Element, Model,
        std::enable_if_t<
            ! std::is_base_of<
                IdentifiedObject<Model>,
                Element
            >::value
        >> : public MapBase<
            Element,
            Model,
            bmi::multi_index_container<
                Element*,
                bmi::indexed_by<
                    bmi::hashed_unique<
                        bmi::const_mem_fun<
                            Element,
                            const Id<Model>&,
                            &Element::id
                        >
                    >
                >
            >
        >
{
    public:
    using MapBase<
    Element,
    Model,
    bmi::multi_index_container<
        Element*,
        bmi::indexed_by<
            bmi::hashed_unique<
                bmi::const_mem_fun<
                    Element,
                    const Id<Model>&,
                    &Element::id
                >
            >
        >
    >
>::MapBase;

        auto& at(const Id<Model>& id) const
        {
            auto item = this->m_map.find(id);
            ISCORE_ASSERT(item != this->m_map.end());
            return **item;
        }
};
