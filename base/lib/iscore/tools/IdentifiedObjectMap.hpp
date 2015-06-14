#pragma once
#include <iscore/tools/IdentifiedObject.hpp>

#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>

namespace bmi = boost::multi_index;

template<class Element, class Model = Element, class Enable = void>
class IdContainer
{

};

template<typename Element, typename Model>
class IdContainer<Element, Model,
        typename
        std::enable_if<
            std::is_base_of<
                IdentifiedObject<Model>,
                Element
            >::value
        >::type>
{
    public:
        using value_type = Element*;
        using model_type = Model;
        auto begin() const { return map.begin(); }
        auto cbegin() const { return map.cbegin(); }
        auto end() const { return map.end(); }
        auto cend() const { return map.cend(); }

        void insert(value_type t)
        { map.insert(t); }

        std::size_t size() const
        { return map.size(); }

        bool empty() const
        { return map.empty(); }

        void remove(value_type t)
        { map.erase(t); }
        void remove(const id_type<model_type>& id)
        { map.erase(id); }

        void clear()
        { map.clear(); }

        auto find(const id_type<model_type>& id) const
        { return map.find(id); }

        auto& get() { return map.template get<0>(); }
        const auto& get() const { return map.template get<0>(); }

        // TODO put some Q_ASSERT in debug mode
        const auto& at(const id_type<model_type>& id) const
        { return *find(id); }

    private:
        bmi::multi_index_container<
            value_type,
            bmi::indexed_by<
                bmi::hashed_unique<
                    bmi::const_mem_fun<
                        IdentifiedObject<Model>,
                        const id_type<model_type>&,
                        &IdentifiedObject<Model>::id
                    >
                >
            >
        > map;
};


template<typename Element, typename Model>
class IdContainer<Element, Model,
        typename
        std::enable_if<
            not std::is_base_of<
                IdentifiedObject<Model>,
                Element
            >::value
        >::type>
{
    public:
        using value_type = Element*;
        using model_type = Model;
        auto begin() const { return map.begin(); }
        auto cbegin() const { return map.cbegin(); }
        auto end() const { return map.end(); }
        auto cend() const { return map.cend(); }

        void insert(value_type t)
        { map.insert(t); }

        std::size_t size() const
        { return map.size(); }

        bool empty() const
        { return map.empty(); }

        void remove(value_type t)
        { map.erase(t); }

        // TODO create one that passes an iterator.
        void remove(const id_type<model_type>& id)
        { map.erase(id); }

        void clear()
        { map.clear(); }

        auto find(const id_type<model_type>& id) const
        { return map.find(id); }

        auto& get() { return map.template get<0>(); }
        const auto& get() const { return map.template get<0>(); }

        const auto& at(const id_type<model_type>& id) const
        { return *find(id); }

    private:
        using map_type = bmi::multi_index_container<
        value_type,
        bmi::indexed_by<
            bmi::hashed_unique<
                bmi::const_mem_fun<
                    Element,
                    const id_type<model_type>&,
                    &Element::id
                >
            >
        >
    >;
        map_type map;
};

template<class Element, typename Model>
auto begin(const IdContainer<Element, Model>& theMap)
{
    return theMap.begin();
}
template<class Element, typename Model>
auto end(const IdContainer<Element, Model>& theMap)
{
    return theMap.end();
}
