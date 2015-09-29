#pragma once
#include <QDataStream>
#include <iscore/document/DocumentInterface.hpp>

template<typename T, typename U>
struct in_relationship
{
    static const constexpr bool value = std::is_base_of<T, U>::value || std::is_base_of<U, T>::value ;
};

/**
 * @brief The Path class is a typesafe wrapper around ObjectPath.
 */
template<typename Object>
class Path
{
        friend bool operator==(const Path& lhs, const Path& rhs)
        {
            return lhs.m_impl == rhs.m_impl;
        }

        friend uint qHash(const Path& obj, uint seed)
        {
          return qHash(obj.m_impl, seed);
        }

        template<typename U>
        friend class Path;
        friend class ObjectPath;

    public:
        // Use this if it is not possible to get a path
        // (for instance because the object does not exist yet)
        struct UnsafeDynamicCreation{};
        Path(const ObjectPath& obj, UnsafeDynamicCreation): m_impl(obj) { }
        Path(ObjectPath&& obj, UnsafeDynamicCreation): m_impl(std::move(obj)) { }

        Path(const Object& obj): m_impl(iscore::IDocument::unsafe_path(safe_cast<const QObject&>(obj))) { }

        template<typename U, std::enable_if_t<in_relationship<U, Object>::value>* = nullptr>
        Path(const Path<U>& other): m_impl(other.m_impl) { }
        template<typename U, std::enable_if_t<in_relationship<U, Object>::value>* = nullptr>
        Path(Path<U>&& other): m_impl(std::move(other.m_impl)) { }

        template<typename U, std::enable_if_t<in_relationship<U, Object>::value>* = nullptr>
        Path& operator=(const Path<U>& other) { m_impl = other.m_impl; return *this; }
        template<typename U, std::enable_if_t<in_relationship<U, Object>::value>* = nullptr>
        Path& operator=(Path<U>&& other) { m_impl = std::move(other.m_impl); return *this; }

        Path() = default;
        Path(const Path&) = default;
        Path(Path&&) = default;
        Path& operator=(const Path&) = default;
        Path& operator=(Path&&) = default;

        Object& find() const
        { return m_impl.find<Object>(); }
        Object* try_find() const
        { return m_impl.try_find<Object>(); }

        const auto& unsafePath() const
        { return m_impl; }
        auto& unsafePath()
        { return m_impl; }

        auto&& moveUnsafePath()
        { return std::move(m_impl); }

        bool valid() const
        { return m_impl.vec().size() > 0; }

    private:
        Path(ObjectPath&& path): m_impl{std::move(path)} { }
        ObjectPath m_impl;
};

namespace iscore
{
namespace IDocument
{

/**
 * @brief path Typesafe path of an object in a document.
 * @param obj The object of which path is to be created.
 * @return A path to the object if it is in a document
 *
 * This function will abort the software if given an object
 * not in a document hierarchy in argument.
 */
template<typename T, std::enable_if_t<
             std::is_base_of<
                 IdentifiedObjectAbstract,
                 T
                >::value
             >* = nullptr
         >
Path<T> path(const T& obj)
{
    static_assert(!std::is_pointer<T>::value, "Don't pass a pointer to path");
    if(obj.m_path_cache.valid())
        return obj.m_path_cache;

    obj.m_path_cache = Path<T>{obj};
    return obj.m_path_cache;
}

template<typename T, std::enable_if_t<
             !std::is_base_of<
                 IdentifiedObjectAbstract,
                 T
                >::value
             >* = nullptr
         >
Path<T> path(const T& obj)
{
    static_assert(!std::is_pointer<T>::value, "Don't pass a pointer to path");
    return obj;
}
}
}
