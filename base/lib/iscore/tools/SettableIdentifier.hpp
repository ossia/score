#pragma once
#include <iscore/tools/std/Optional.hpp>
#include <QDebug>
#include <QPointer>
#include <iscore/tools/Todo.hpp>

template<typename T>
class NotifyingMap;
template<typename Element, typename Model, typename Map>
class IdContainer;
template<typename T>
class IdentifiedObject;
/**
 * @brief The id_base_t class
 *
 * The base identifier type.
 */
template<typename tag, typename impl>
class id_base_t
{
        friend tag;
        friend class NotifyingMap<tag>;
        friend class IdentifiedObject<tag>;

        // TODO Try to only have Map as a template type here
        template<typename Element, typename Model, typename Map> friend class IdContainer;
    public:
        using value_type = impl;
        explicit id_base_t() = default;
        id_base_t(const id_base_t & other):
            m_id{other.m_id}
        {

        }

        id_base_t(id_base_t && other):
            m_id{std::move(other.m_id)}
        {

        }

        id_base_t& operator=(const id_base_t & other)
        {
            m_id = other.m_id;
            m_ptr.clear();
            return *this;
        }

        id_base_t& operator=(id_base_t && other)
        {
            m_id = other.m_id;
            m_ptr.clear();
            return *this;
        }

        // TODO check if when an id is returned by value,
        // the pointer gets copied correctly
        explicit id_base_t(value_type val) : m_id {std::move(val)} { }

        explicit id_base_t(tag& element):
            m_ptr{&element},
            m_id{element.id()}
        {

        }

        id_base_t& operator=(tag& element)
        {
            m_ptr = &element;
            m_id = element.id();

            return *this;
        }

        friend bool operator== (const id_base_t& lhs, const id_base_t& rhs)
        {
            return lhs.m_id == rhs.m_id;
        }

        friend bool operator!= (const id_base_t& lhs, const id_base_t& rhs)
        {
            return lhs.m_id != rhs.m_id;
        }

        friend bool operator< (const id_base_t& lhs, const id_base_t& rhs)
        {
            return *lhs.val() < *rhs.val();
        }

        explicit operator bool() const
        {
            return bool(m_id);
        }

        explicit operator value_type() const
        {
            return m_id;
        }

        const value_type& val() const
        {
            return m_id;
        }

        void setVal(value_type&& val)
        {
            m_id = val;
        }

        void unset()
        {
            m_id = value_type();
        }

    private:
        mutable QPointer<QObject> m_ptr;
        value_type m_id {};
};

template<typename tag, typename impl>
using optional_tagged_id = id_base_t<tag, optional<impl>>;

template<typename tag>
using optional_tagged_int32_id = optional_tagged_id<tag, int32_t>;

template<typename tag>
using Id = optional_tagged_int32_id<tag>;

template<typename tag>
struct id_hash
{
    std::size_t operator()(const Id<tag>& id) const
    {
        return std::hash<int32_t>()(*id.val());
    }
};

template<typename T>
uint qHash(const Id<T>& id, uint seed)
{
    return qHash(*id.val(), seed);
}

template<typename tag>
QDebug operator<< (QDebug dbg, const Id<tag>& c)
{
    if(c.val())
    {
        dbg.nospace() << *c.val();
    }
    else
    {
        dbg.nospace() << "Not set";
    }

    return dbg.space();
}
