#pragma once
#include <iscore/plugins/customfactory/UuidKey.hpp>
#include <iscore_lib_base_export.h>

namespace iscore
{
// Base class for factories of elements whose type is not part of the base application.

class FactoryInterfaceBase;
using AbstractFactoryKey = UuidKey<FactoryInterfaceBase>;

class ISCORE_LIB_BASE_EXPORT FactoryInterfaceBase
{
    public:
        virtual ~FactoryInterfaceBase();

        /**
         * @brief abstractFactoryKey
         * @return The general factory (ProtocolFactory, ProcessFactory)
         */
        virtual const AbstractFactoryKey& abstractFactoryKey() const = 0;
};

// Keys for the sub-classes to identify themselves.
template<typename Key>
class ISCORE_LIB_BASE_EXPORT FactoryKeyInterface
{
    public:
        virtual ~FactoryKeyInterface() = default;

        // TODO protected:
        virtual const Key& concreteFactoryKey() const = 0;
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
            return static_cast<const FactoryKeyInterface<Key_T>*>(this)->concreteFactoryKey();
        }
};

}

#define ISCORE_ABSTRACT_FACTORY_DECL(Type, Uuid) \
    public: \
    static const iscore::AbstractFactoryKey& static_abstractFactoryKey() { \
        static const iscore::AbstractFactoryKey s{boost::uuids::string_generator{}(Uuid)}; \
        return s; \
    } \
    \
    const iscore::AbstractFactoryKey& abstractFactoryKey() const final override { \
        return static_abstractFactoryKey(); \
    } \
    using object_type = Type; \
    using ConcreteFactoryKey = UuidKey<Type>; \
    private:



// ConcreteFactoryKey should be defined in the subclass
#define ISCORE_CONCRETE_FACTORY_DECL(Uuid) \
    private: \
    static const auto& static_concreteFactoryKey() { \
        static const ConcreteFactoryKey id{boost::uuids::string_generator{}(Uuid)}; \
        return id; \
    } \
    \
    const ConcreteFactoryKey& concreteFactoryKey() const final override { \
        return static_concreteFactoryKey(); \
    }

