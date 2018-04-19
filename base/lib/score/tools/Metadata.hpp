#pragma once
#include <score/plugins/customfactory/UuidKey.hpp>
#define EMPTY_MACRO

/**
 * \class Metadata
 * \brief Static metadata implementation
 *
 * This class is used to provide various kind of
 * metadata on the classes in the software.
 * It is akin to a compile-time `map<key, value>`.
 *
 * The usage looks like :
 * \code
 * QString name = Metadata<PrettyName_k, MyType>::get();
 * \endcode
 *
 * The metadata has to be registered, either by hand :
 * \code
 * template<>
 * struct Metadata<PrettyName_k, MyType>
 * {
 *   static auto get() { return "some name"; }
 * };
 * \endcode
 *
 * Or by using one of the various macros provided. Custom types
 * are encouraged to provide macros to easily aggregate all the metadata
 * required for them.
 *
 * \note Since the mechanism works by template specialization,
 * it is necessary to put the macros or implementation outside of any
 * namespace.
 */
template <typename Metadata_T, typename Object>
struct Metadata;

/**
 * \class ObjectKey_k
 * \brief Metadata to get the key part of ObjectIdentifier.
 */
class ObjectKey_k;

/**
 * \class PrettyName_k
 * \brief Metadata to get the name that will be shown in the user interface.
 */
class PrettyName_k;

/**
 * \class UndoName_k
 * \brief Metadata to get the name that will be shown in the undo-redo panel.
 */
class UndoName_k;

/**
 * \class Description_k
 * \brief Metadata to get the description that will be shown in the undo-redo
 * panel.
 */
class Description_k;

/**
 * \class Category_k
 * \brief Metadata to categorize objects: curves, audio, etc
 */
class Category_k;

/**
 * \class Tags_k
 * \brief Metadata to associate tags to objects
 */
class Tags_k;

/**
 * \class Description_k
 * \brief Metadata to identify a type for factories.
 */
class ConcreteKey_k;

/**
 * \class Json_k
 * \brief Metadata to give the JSON key that matches a given type.
 *
 * It is used by the \ref VariantSerialization mechanism.
 */
class Json_k;

/**
 * \macro AUTO_METADATA
 * \brief Easily generate a generic metadata type.
 * \param Export The export macro (e.g. SCORE_LIB_BASE_EXPORT) if the metadata
 * is to be accessed across plug-ins. \param Model The type on which the
 * metadata applies. \param Key The metadata key. \param Text The metadata
 * value.
 */
#define AUTO_METADATA(Export, Model, Key, Text) \
  template <>                                   \
  struct Export Metadata<Key, Model>            \
  {                                             \
    static auto get()                           \
    {                                           \
      return Text;                              \
    }                                           \
  };

#define TYPED_METADATA(Export, Model, Key, Type, Value) \
  template <>                                           \
  struct Export Metadata<Key, Model>                    \
  {                                                     \
    static Type get()                                   \
    {                                                   \
      return Value;                                     \
    }                                                   \
  };

#define TR_TEXT_METADATA(Export, Model, Key, Text) \
  AUTO_METADATA(Export, Model, Key, QObject::tr(Text))
#define LIT_TEXT_METADATA(Export, Model, Key, Text) \
  AUTO_METADATA(Export, Model, Key, QStringLiteral(Text))

#define OBJECTKEY_METADATA(Export, Model, ObjectKey) \
  LIT_TEXT_METADATA(Export, Model, ObjectKey_k, ObjectKey)

#define DESCRIPTION_METADATA(Export, Model, Text) \
  TR_TEXT_METADATA(Export, Model, Description_k, Text)
#define CATEGORY_METADATA(Export, Model, Text) \
  TR_TEXT_METADATA(Export, Model, Category_k, Text)
#define TAGS_METADATA(Export, Model, Text) \
  TYPED_METADATA(Export, Model, Tags_k, QStringList, Text)

#define UNDO_NAME_METADATA(Export, Model, Text) \
  TR_TEXT_METADATA(Export, Model, UndoName_k, Text)

#define DEFAULT_MODEL_METADATA(Model, Description) \
  OBJECTKEY_METADATA(EMPTY_MACRO, Model, #Model)   \
  DESCRIPTION_METADATA(EMPTY_MACRO, Model, Description)

#define JSON_METADATA(Type, Text) LIT_TEXT_METADATA(, Type, Json_k, Text)

#define STATIC_METADATA(Export, Model, Key, Type, Value) \
  template <>                                            \
  struct Export Metadata<Key, Model>                     \
  {                                                      \
    static Q_DECL_RELAXED_CONSTEXPR Type get()           \
    {                                                    \
      const Q_DECL_RELAXED_CONSTEXPR Type k{Value};      \
      return k;                                          \
    }                                                    \
  };

#define UUID_METADATA(Export, Factory, Model, Uuid) \
  STATIC_METADATA(Export, Model, ConcreteKey_k, UuidKey<Factory>, Uuid)

#define MODEL_METADATA(Export, Factory, Model, Uuid, ObjectKey, PrettyName) \
  OBJECTKEY_METADATA(Export, Model, ObjectKey)                              \
  UUID_METADATA(Export, Factory, Model, Uuid)                               \
  TR_TEXT_METADATA(Export, Model, PrettyName_k, PrettyName)
