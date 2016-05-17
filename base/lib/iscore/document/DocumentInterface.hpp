#pragma once
#include <iscore/tools/ObjectPath.hpp>
#include <type_traits>
#include <vector>

class QObject;
//#include <iscore/tools/ModelPath.hpp>

namespace iscore
{
class CommandStack;
class Document;
class DocumentDelegateModelInterface;
class DocumentDelegatePresenterInterface;
struct DocumentContext;

namespace IDocument
{
/**
 * @brief documentFromObject
 * @param obj an object in the application hierarchy
 *
 * @return the Document parent of the object or nullptr.
 */
ISCORE_LIB_BASE_EXPORT Document* documentFromObject(const QObject* obj);
ISCORE_LIB_BASE_EXPORT Document* documentFromObject(const QObject& obj);
ISCORE_LIB_BASE_EXPORT const DocumentContext& documentContext(const QObject& obj);

/**
 * @brief pathFromDocument
 * @param obj an object in the application hierarchy
 *
 * @return The path between a Document and this object.
 *
 * These functions are not type-safe, hence use them only if there
 * is no other choice (e.g. storing the path to objects of different types)...
 */
ISCORE_LIB_BASE_EXPORT ObjectPath unsafe_path(QObject const * const& obj);
ISCORE_LIB_BASE_EXPORT ObjectPath unsafe_path(const QObject& obj);

//// Various getters ////

// Presenter of a document plugin.
ISCORE_LIB_BASE_EXPORT DocumentDelegatePresenterInterface& presenterDelegate_generic(const Document& d);

template<typename T> T& presenterDelegate(const Document& d)
{
    return safe_cast<T&>(presenterDelegate_generic(d));
}

template<typename T,
         std::enable_if_t<std::is_base_of<DocumentDelegatePresenterInterface, T>::value>* = nullptr>
T& get(const Document& d)
{
    return presenterDelegate<T> (d);
}


// Model of a document plugin
// First if we are sure
ISCORE_LIB_BASE_EXPORT DocumentDelegateModelInterface& modelDelegate_generic(const Document& d);

template<typename T> T& modelDelegate(const Document& d)
{
    return safe_cast<T&>(modelDelegate_generic(d));
}

template<typename T,
         std::enable_if_t<std::is_base_of<DocumentDelegateModelInterface, T>::value>* = nullptr>
T& get(const Document& d)
{
    return modelDelegate<T> (d);
}

// And then if we are not

template<typename T> T* try_modelDelegate(const Document& d)
{
    return dynamic_cast<T*>(&modelDelegate_generic(d));
}

template<typename T,
         std::enable_if_t<std::is_base_of<DocumentDelegateModelInterface, T>::value>* = nullptr>
T* try_get(const Document& d)
{
    return try_modelDelegate<T> (d);
}
}
}
