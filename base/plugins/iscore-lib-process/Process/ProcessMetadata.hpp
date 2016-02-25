#pragma once
#include <iscore/tools/Metadata.hpp>
#include <iscore/plugins/customfactory/UuidKey.hpp>
#include <QObject>

namespace Process
{
class ProcessFactory;
class StateProcessFactory;
}
#define PROCESS_FACTORY_METADATA(Export, Model, Uuid) \
template<> \
struct Export Metadata< \
        ConcreteFactoryKey_k, \
        Model> \
{ \
        static const auto& get() \
        { \
            static const UuidKey<Process::ProcessFactory> k{Uuid}; \
            return k; \
        } \
};

#define PROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName) \
 OBJECTKEY_METADATA(Export, Model, ObjectKey) \
 PROCESS_FACTORY_METADATA(Export, Model, Uuid) \
template<> \
struct Export Metadata< \
        PrettyName_k, \
        Model> \
{ \
        static auto get() \
        { \
            return QObject::tr(PrettyName); \
        } \
};



#define STATEPROCESS_FACTORY_METADATA(Export, Model, Uuid) \
template<> \
struct Export Metadata< \
        ConcreteFactoryKey_k, \
        Model> \
{ \
        static const auto& get() \
        { \
            static const UuidKey<Process::StateProcessFactory> k{Uuid}; \
            return k; \
        } \
};

#define STATEPROCESS_METADATA(Export, Model, Uuid, ObjectKey, PrettyName) \
 OBJECTKEY_METADATA(Export, Model, ObjectKey) \
 STATEPROCESS_FACTORY_METADATA(Export, Model, Uuid) \
template<> \
struct Export Metadata< \
        PrettyName_k, \
        Model> \
{ \
        static auto get() \
        { \
            return QObject::tr(PrettyName); \
        } \
};
