#pragma once

#include <iscore/tools/std/ConstexprString.hpp>
/*
#if !defined(USE_CONSTEXPR_STRING)
#define ISCORE_METADATA(TheClass) public: static const std::string className; private:
#define ISCORE_METADATA_IMPL(TheClass) const std::string TheClass::className{ #TheClass };
#else
#define ISCORE_METADATA(TheClass) public: static constexpr auto className = #TheClass##_S; private:
#define ISCORE_METADATA_IMPL(TheClass) template<> constexpr const char decltype(#TheClass##_S) :: _data[]; constexpr decltype(#TheClass##_S) TheClass :: className;
#endif
*/
#define EMPTY_MACRO

template<typename Metadata_T, typename Object>
struct Metadata;

class ObjectKey_k;
class PrettyName_k;
class UndoName_k;
class Description_k;
class ConcreteFactoryKey_k;

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


#define OBJECTKEY_METADATA(Export, Model, ObjectKey) \
    TEXT_METADATA(Export, Model, ObjectKey_k, ObjectKey)

#define DESCRIPTION_METADATA(Export, Model, Text) \
    TR_TEXT_METADATA(Export, Model, Description_k, Text)

#define UNDO_NAME_METADATA(Export, Model, Text) \
    TR_TEXT_METADATA(Export, Model, UndoName_k, Text)


#define DEFAULT_MODEL_METADATA(Model, Description) \
    OBJECTKEY_METADATA(EMPTY_MACRO, Model, #Model) \
    DESCRIPTION_METADATA(EMPTY_MACRO, Model, Description)
