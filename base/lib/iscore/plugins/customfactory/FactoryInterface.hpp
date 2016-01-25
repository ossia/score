#pragma once
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore_lib_base_export.h>

namespace iscore
{
// Base class for factories of elements whose type is not part of the base application.

class FactoryBaseTag{};
using FactoryBaseKey = StringKey<FactoryBaseTag>;

class ISCORE_LIB_BASE_EXPORT FactoryInterfaceBase
{
    public:
        virtual ~FactoryInterfaceBase();

        /**
         * @brief factoryKey
         * @return The general factory (ProtocolFactory, ProcessFactory)
         */
        virtual const FactoryBaseKey& factoryKey() const = 0;
};

// Keys for the sub-classes to identify themselves.
template<typename Key>
class ISCORE_LIB_BASE_EXPORT FactoryKeyInterface
{
    public:
        virtual ~FactoryKeyInterface() = default;

        // TODO protected:
        virtual const Key& key_impl() const = 0;
};

template <typename... Keys>
class GenericFactoryInterface_Base;

template<typename Key, typename... Keys>
class ISCORE_LIB_BASE_EXPORT GenericFactoryInterface_Base<Key, Keys...> :
        public FactoryKeyInterface<Key>,
        public GenericFactoryInterface_Base<Keys...>
{
    public:
        virtual ~GenericFactoryInterface_Base() = default;
};

template<>
class ISCORE_LIB_BASE_EXPORT GenericFactoryInterface_Base<> :
        public iscore::FactoryInterfaceBase
{
    public:
        virtual ~GenericFactoryInterface_Base() = default;
};

template<typename... Keys>
class ISCORE_LIB_BASE_EXPORT GenericFactoryInterface : public GenericFactoryInterface_Base<Keys...>
{
    public:
        template<typename Key_T>
        const Key_T& key() const
        {
            return static_cast<const FactoryKeyInterface<Key_T>*>(this)->key_impl();
        }
};

}

#define ISCORE_FACTORY_DECL(Type, Uuid) \
    public: \
    static const iscore::FactoryBaseKey& staticFactoryKey() { \
        static const iscore::FactoryBaseKey s{Uuid}; \
        return s; \
    } \
    \
    const iscore::FactoryBaseKey& factoryKey() const final override { \
        return staticFactoryKey(); \
    } \
    using object_type = Type; \
    private:

