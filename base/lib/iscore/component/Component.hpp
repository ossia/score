#pragma once
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/plugins/customfactory/UuidKey.hpp>
namespace iscore
{
#define COMPONENT_METADATA(Uuid) \
    public: \
    static constexpr Component::Key static_key() { \
        return_uuid(Uuid); \
    } \
    \
    Component::Key key() const final override { \
      return static_key(); \
    } \
    private:


class ISCORE_LIB_BASE_EXPORT Component :
        public IdentifiedObject<iscore::Component>
{
    public:
        using IdentifiedObject<iscore::Component>::IdentifiedObject;
        using Key = UuidKey<iscore::Component>;
        virtual Key key() const = 0;

        virtual ~Component();
};

template<typename Component_T, typename System_T>
class SystemComponent :
        public Component_T
{
    public:
        template<typename... Args>
        SystemComponent(System_T& sys, Args&&... args):
            Component_T{std::forward<Args>(args)...},
            m_system{sys}
        {

        }

        System_T& system() const
        { return m_system; }

    private:
        System_T& m_system;
};

template<typename System_T>
using GenericComponent = iscore::SystemComponent<iscore::Component, System_T>;

using Components = NotifyingMap<iscore::Component>;

template<typename T>
auto& component(const iscore::Components& c)
{
    static_assert(T::is_unique, "Components must be unique to use getComponent");

    auto it = find_if(c, [] (auto& other) {
        return T::static_key() == other.key();
    });

    ISCORE_ASSERT(it != c.end());
    return static_cast<T&>(*it);
}

template<typename T>
auto findComponent(const iscore::Components& c)
{
    static_assert(T::is_unique, "Components must be unique to use getComponent");

    auto it = find_if(c, [] (auto& other) {
        return T::static_key() == other.key();
    });

    if(it != c.end())
    {
        return static_cast<T*>(&*it);
    }
    else
    {
        return (T*)nullptr;
    }
}
}
