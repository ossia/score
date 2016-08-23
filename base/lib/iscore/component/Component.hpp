#pragma once
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/plugins/customfactory/UuidKey.hpp>
namespace iscore
{
#define ABSTRACT_COMPONENT_METADATA(Type, Uuid) \
    public: \
    using base_component_type = Type; \
    \
    static Q_RELAXED_CONSTEXPR Component::Key static_key() { \
        return_uuid(Uuid); \
    } \
    \
    static Q_RELAXED_CONSTEXPR bool base_key_match(Component::Key other) { \
      return static_key() == other; \
    } \
    private:

#define COMPONENT_METADATA(Uuid) \
    public: \
    static Q_RELAXED_CONSTEXPR Component::Key static_key() { \
        return_uuid(Uuid); \
    } \
    \
    Component::Key key() const final override { \
      return static_key(); \
    } \
    \
    bool key_match(Component::Key other) const final override { \
      return static_key() == other || base_component_type::base_key_match(other); \
    } \
    private:

#define COMMON_COMPONENT_METADATA(Uuid) \
    public: \
    static Q_RELAXED_CONSTEXPR Component::Key static_key() { \
        return_uuid(Uuid); \
    } \
    \
    Component::Key key() const final override { \
      return static_key(); \
    } \
    \
    bool key_match(Component::Key other) const final override { \
      return static_key() == other; \
    } \
    private:

class ISCORE_LIB_BASE_EXPORT Component :
        public IdentifiedObject<iscore::Component>
{
    public:
        using IdentifiedObject<iscore::Component>::IdentifiedObject;
        using Key = UuidKey<iscore::Component>;
        virtual Key key() const = 0;
        virtual bool key_match(Key other) const = 0;

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
        return other.key_match(T::static_key());
    });

    ISCORE_ASSERT(it != c.end());
    return static_cast<T&>(*it);
}

template<typename T>
auto findComponent(const iscore::Components& c)
{
    static_assert(T::is_unique, "Components must be unique to use getComponent");

    auto it = find_if(c, [] (auto& other) {
        return other.key_match(T::static_key());
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
