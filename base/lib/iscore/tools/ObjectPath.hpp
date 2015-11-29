#pragma once
#include <iscore/tools/ObjectIdentifier.hpp>
#include <qglobal.h>
#include <qhash.h>
#include <qpointer.h>
#include <qstring.h>
#include <algorithm>
#include <initializer_list>
#include <type_traits>
#include <vector>

class QObject;

/**
 * @brief The ObjectPath class
 *
 * A path in the QObject hierarchy, using names and identifiers.
 * See @c{NamedObject} and @c{IdentifiedObject}.
 *
 * If the object id is not known at compile time a path may be created using ObjectPath::pathFromObject.
 *
 * Example:
 * @code
 *	ObjectPath p{ {"MyObject", {}}, {"SomeSubObjectInACollection", 27} };
 * @endcode
 *
 * Note : this class is mostly superseded by Path<T> which adds type-safety.
*/
class ObjectPath
{
        friend ObjectIdentifierVector::iterator begin(ObjectPath& path)
        {
            return path.m_objectIdentifiers.begin();
        }

        friend ObjectIdentifierVector::iterator end(ObjectPath& path)
        {
            return path.m_objectIdentifiers.end();
        }

        friend bool operator== (const ObjectPath& lhs, const ObjectPath& rhs)
        {
            return lhs.m_objectIdentifiers == rhs.m_objectIdentifiers;
        }

    public:
        ObjectPath() = default;
        QString toString() const;

        explicit ObjectPath(const std::vector<ObjectIdentifier>& vec) :
            m_objectIdentifiers {vec}
        {
        }

        explicit ObjectPath(std::vector<ObjectIdentifier>&& vec) :
            m_objectIdentifiers {std::move(vec)}
        {
        }

        explicit ObjectPath(std::initializer_list<ObjectIdentifier> lst) :
            m_objectIdentifiers(lst)
        {
        }

        ObjectPath(const ObjectPath& obj):
            m_objectIdentifiers{obj.m_objectIdentifiers}
        {
        }

        ObjectPath(ObjectPath&& obj):
            m_objectIdentifiers{std::move(obj.m_objectIdentifiers)}
        {
        }

        ObjectPath& operator= (ObjectPath && obj)
        {
            m_objectIdentifiers = std::move(obj.m_objectIdentifiers);
            m_cache.clear();
            return *this;
        }

        ObjectPath& operator= (const ObjectPath& obj)
        {
            m_objectIdentifiers = obj.m_objectIdentifiers;
            m_cache.clear();
            return *this;
        }

        static ObjectPath pathBetweenObjects(const QObject* const parent_obj,
                                             const QObject* target_object);

        /**
         * @brief find the object described by the ObjectPath
         * @return A pointer to the object, or nullptr if it is not found
         *
         * This search starts from qApp.
         * @todo (maybe) a way to specify custom ways of finding an object
         * (for instance if obj->blurb() == Ding::someDing)
         * @todo search starting from another object, for more performance.
         */
        template<class T>
        T& find() const
        {
            // First see if the pointer is still loaded in the cache.
            if(!m_cache.isNull())
            {
                return *safe_cast<T*>(m_cache.data());
            }
            else // Load it by hand
            {
                auto ptr = safe_cast<typename std::remove_const<T>::type*>(find_impl());
                m_cache = ptr;
                return *ptr;
            }
        }

        /**
         * @brief try_find Tries to find an object
         *
         * @return null if the object does not exist.
         */
        template<class T>
        T* try_find() const
        {
            try
            {
                if(!m_cache.isNull())
                {
                    return safe_cast<T*>(m_cache.data());
                }
                else // Load it by hand
                {
                    auto ptr = static_cast<typename std::remove_const<T>::type*>(find_impl_unsafe());
                    m_cache = ptr;
                    return ptr;
                }
            }
            catch(...)
            {
                return nullptr;
            }
        }

        const ObjectIdentifierVector& vec() const
        { return m_objectIdentifiers; }

        ObjectIdentifierVector& vec()
        { return m_objectIdentifiers; }

    private:
        // Throws
        QObject* find_impl() const;

        // Returns nullptr
        QObject* find_impl_unsafe() const;

        ObjectIdentifierVector m_objectIdentifiers;
        mutable QPointer<QObject> m_cache;
};


inline
uint qHash(const ObjectPath& obj, uint seed)
{
  return qHash(obj.toString(), seed);
}

Q_DECLARE_METATYPE(ObjectPath)
