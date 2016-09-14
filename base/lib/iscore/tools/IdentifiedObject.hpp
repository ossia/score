#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/utilsCPP11.hpp>
#include <iscore/tools/ModelPath.hpp>

/**
 * @brief The IdentifiedObject class
 *
 * An object with an unique identifier. This identifier
 * is used to find objects in a path.
 *
 * A class should only have a single child of the same type with a given identifier
 * since QObject::findChild is used.
 *
 */
template<typename model>
class IdentifiedObject : public IdentifiedObjectAbstract
{
    public:
        using model_type = model;
        using id_type = Id<model>;
        template<typename... Args>
        IdentifiedObject(id_type id,
                         Args&& ... args) :
            IdentifiedObjectAbstract {std::forward<Args> (args)...},
            m_id {std::move(id)}
        {
            m_id.m_ptr = this;
        }

        template<typename ReaderImpl>
        IdentifiedObject(Deserializer<ReaderImpl>& v, QObject* parent) :
            IdentifiedObjectAbstract {v, parent}
        {
            v.writeTo(*this);
            m_id.m_ptr = this;
        }

        virtual ~IdentifiedObject()
        {

        }

        const id_type& id() const
        {
            return m_id;
        }

        int32_t id_val() const final override
        {
            return m_id.val();
        }

        void setId(const id_type& id)
        {
            m_id = id;
        }
        void setId(id_type&& id)
        {
            m_id = std::move(id);
        }

        mutable Path<model> m_path_cache; // TODO see http://stackoverflow.com/questions/32987869/befriending-of-function-template-with-enable-if to put in private
    private:
        id_type m_id {};
};


template<typename model>
std::size_t hash_value(const Id<model>& id)
{
    return id.val();
}

template<typename T, typename U>
bool operator==(const T* obj, const Id<U>& id)
{
    return obj->id() == id;
}

template<typename T, typename U, typename = decltype(std::declval<T>().id())>
bool operator==(const T& obj, const Id<U>& id)
{
    return obj.id() == id;
}

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
             >*
         >
Path<T> path(const T& obj)
{
    static_assert(!std::is_pointer<T>::value, "Don't pass a pointer to path");
    if(obj.m_path_cache.valid())
        return obj.m_path_cache;

    obj.m_path_cache = Path<T>{iscore::IDocument::unsafe_path(safe_cast<const QObject&>(obj)), {}};
    return obj.m_path_cache;
}

template<typename T, std::enable_if_t<
             !std::is_base_of<
                 IdentifiedObjectAbstract,
                 T
                >::value
             >*
         >
Path<T> path(const T& obj)
{
    static_assert(!std::is_pointer<T>::value, "Don't pass a pointer to path");
    return Path<T>{iscore::IDocument::unsafe_path(safe_cast<const QObject&>(obj)), {}};
}
}
}
