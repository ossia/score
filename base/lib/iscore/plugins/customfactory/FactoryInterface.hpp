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
        virtual AbstractFactoryKey abstractFactoryKey() const = 0;
};

// Keys for the sub-classes to identify themselves.
template<typename Key>
class FactoryInterface_T : public FactoryInterfaceBase
{
    public:
        virtual ~FactoryInterface_T() = default;

        using ConcreteFactoryKey = Key;
        virtual Key concreteFactoryKey() const = 0;

};

template<typename T>
using AbstractFactory = FactoryInterface_T<UuidKey<T>>;
}

#define ISCORE_ABSTRACT_FACTORY(Uuid) \
    public: \
    static constexpr iscore::AbstractFactoryKey static_abstractFactoryKey() { \
        return_uuid(Uuid); \
    } \
    \
    iscore::AbstractFactoryKey abstractFactoryKey() const final override { \
        return static_abstractFactoryKey(); \
    } \
    private:



// ConcreteFactoryKey should be defined in the subclass
#define ISCORE_CONCRETE_FACTORY(Uuid) \
    public: \
    static auto static_concreteFactoryKey() { \
        return_uuid(Uuid); \
    } \
    \
    ConcreteFactoryKey concreteFactoryKey() const final override { \
        return static_concreteFactoryKey(); \
    }\
    private:

