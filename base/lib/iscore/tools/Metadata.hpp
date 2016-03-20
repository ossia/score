#pragma once
#include <iscore/plugins/customfactory/UuidKey.hpp>
#define EMPTY_MACRO

template<typename Metadata_T, typename Object>
struct Metadata;

class ObjectKey_k;
class PrettyName_k;
class UndoName_k;
class Description_k;
class ConcreteFactoryKey_k;
class Json_k;

#define AUTO_METADATA(Export, Model, Key, Text) \
    template<> \
    struct Export Metadata< \
            Key, \
            Model> \
    { \
            static auto get() \
            { \
                return Text; \
            } \
    };


#define TYPED_METADATA(Export, Model, Key, Type, Value) \
    template<> \
    struct Export Metadata< \
            Key, \
            Model> \
    { \
            static Type get() \
            { \
                return Text; \
            } \
    };


#define TR_TEXT_METADATA(Export, Model, Key, Text) \
    AUTO_METADATA(Export, Model, Key, QObject::tr(Text))
#define LIT_TEXT_METADATA(Export, Model, Key, Text) \
    AUTO_METADATA(Export, Model, Key, QStringLiteral(Text))


#define OBJECTKEY_METADATA(Export, Model, ObjectKey) \
    LIT_TEXT_METADATA(Export, Model, ObjectKey_k, ObjectKey)

#define DESCRIPTION_METADATA(Export, Model, Text) \
    TR_TEXT_METADATA(Export, Model, Description_k, Text)

#define UNDO_NAME_METADATA(Export, Model, Text) \
    TR_TEXT_METADATA(Export, Model, UndoName_k, Text)


#define DEFAULT_MODEL_METADATA(Model, Description) \
    OBJECTKEY_METADATA(EMPTY_MACRO, Model, #Model) \
    DESCRIPTION_METADATA(EMPTY_MACRO, Model, Description)

#define JSON_METADATA(Type, Text) \
    LIT_TEXT_METADATA(, Type, Json_k, Text)

// TODO use thread_local ??
#define STATIC_METADATA(Export, Model, Key, Type, Value) \
    template<> \
    struct Export Metadata< \
            Key, \
            Model> \
    { \
            static const auto& get() \
            { \
                static const Type k{Value}; \
                return k; \
            } \
    };

#define UUID_METADATA(Export, Factory, Model, Uuid) \
    STATIC_METADATA(Export, Model, ConcreteFactoryKey_k, UuidKey<Factory>, Uuid)
