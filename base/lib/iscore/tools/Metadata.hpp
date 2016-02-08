#pragma once
#define EMPTY_MACRO

template<typename Metadata_T, typename Object>
struct Metadata;

class ObjectKey_k;
class PrettyName_k;
class UndoName_k;
class Description_k;
class ConcreteFactoryKey_k;
class Json_k;

#define TEXT_METADATA(Export, Model, Key, Text) \
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

#define TR_TEXT_METADATA(Export, Model, Key, Text) \
    TEXT_METADATA(Export, Model, Key, QObject::tr(Text))
#define LIT_TEXT_METADATA(Export, Model, Key, Text) \
    TEXT_METADATA(Export, Model, Key, QStringLiteral(Text))


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
