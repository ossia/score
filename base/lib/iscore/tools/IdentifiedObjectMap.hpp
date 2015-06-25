#pragma once
#include <iscore/tools/IdentifiedObject.hpp>

#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>

// This file contains a fast map for items based on their identifier,
// based on boost's multi-index maps.

namespace bmi = boost::multi_index;

template<class Element, class Model = Element, class Enable = void>
class IdContainer
{

};

template<typename Element, typename Model, typename Map>
class MapBase
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

#ifdef ISCORE_DEBUG
        const auto& at(const id_type<model_type>& id) const
        {
            auto item = find(id);
            if(item == map.end())
            {
                qDebug("FAIL");
            }
            Q_ASSERT(item != map.end());
            return *item;
        }
#else
        const auto& at(const id_type<model_type>& id) const
        { return *find(id); }
#endif

    private:
        Map map;
};

// We have to write two implementations since const_mem_fun does not handle inheritance.

// This map is for classes which directly inherit from
// IdentifiedObject<T> and don't have an id() method by themselves.
template<typename Element, typename Model>
class IdContainer<Element, Model,
        typename
        std::enable_if<
            std::is_base_of<
                IdentifiedObject<Model>,
                Element
            >::value
        >::type> : public MapBase<
            Element,
            Model,
            bmi::multi_index_container<
                Element*,
                bmi::indexed_by<
                    bmi::hashed_unique<
                        bmi::const_mem_fun<
                            IdentifiedObject<Model>,
                            const id_type<Model>&,
                            &IdentifiedObject<Model>::id
                        >
                    >
                >
            >
        >
{

};


// This map is for classes which directly have an id() method
// like a Presenter whose id() would return its model's.
template<typename Element, typename Model>
class IdContainer<Element, Model,
        typename
        std::enable_if<
            not std::is_base_of<
                IdentifiedObject<Model>,
                Element
            >::value
        >::type> : public MapBase<
            Element,
            Model,
            bmi::multi_index_container<
                Element*,
                bmi::indexed_by<
                    bmi::hashed_unique<
                        bmi::const_mem_fun<
                            Element,
                            const id_type<Model>&,
                            &Element::id
                        >
                    >
                >
            >
        >
{
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
