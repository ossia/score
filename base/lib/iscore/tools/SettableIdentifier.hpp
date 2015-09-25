#pragma once
#include <boost/optional.hpp>
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

        // TODO Try to only have Map as a tempalte type here
        template<typename Element, typename Model, typename Map> friend class IdContainer;
    public:
        using value_type = impl;
        explicit id_base_t() = default;
        // TODO check if then an id is returned by value,
        // the pointer getscopied correctly
        explicit id_base_t(value_type val) : m_id {val} { }

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
using optional_tagged_id = id_base_t<tag, boost::optional<impl>>;

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
    return qHash(id.val().get(), seed);
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


/**
 * @brief The Cache class
 *
 * Allows to cache an object along with an id.
 *
 * TESTME
 * MOVEME
 *
 * TODO instead we should have it be a "large object
 */
template<typename T, typename Obj_Id_T = T>
class Cache
{
        friend class NotifyingMap<T>;
        friend class NotifyingMap<Obj_Id_T>;
    public:
        using local_id_type = Id<Obj_Id_T>;
        Cache() = default;
        Cache(const Cache&) = default;
        Cache(Cache&&) = default;
        Cache& operator=(const Cache&) = default;
        Cache& operator=(Cache&&) = default;

        Cache(T& element):
            m_ptr{&element},
            m_id{element.id()}
        {

        }

        Cache(const local_id_type& id):
            m_id{id}
        {

        }

        Cache& operator=(T& element)
        {
            m_ptr = &element;
            m_id = element.id();

            return *this;
        }

        Cache& operator=(const local_id_type& id)
        {
            m_ptr.clear();
            m_id = id;

            return *this;
        }

        operator bool() const
        { return bool(m_ptr); }

        operator const local_id_type&() const
        { return m_id; }

        T& get() const
        {
            ISCORE_ASSERT(m_ptr);
            return *m_ptr;
        }

        T& operator*() const
        { return get(); }

        const local_id_type& id() const
        { return m_id; }

    private:
        mutable QPointer<T> m_ptr;
        local_id_type m_id;
};



template<typename T, typename U>
bool operator==(const T* obj, const Cache<U>& cache)
{
    return obj->id() == cache.id();
}

template<typename T, typename U,std::enable_if_t<! std::is_pointer<std::decay_t<T>>::value>* = nullptr>
bool operator==(const T& obj, const Cache<U>& cache)
{
    return obj.id() == cache.id();
}
