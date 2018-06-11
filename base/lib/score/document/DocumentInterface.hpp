#pragma once
#include <score/model/path/ObjectPath.hpp>
#include <type_traits>
#include <vector>

class QObject;
namespace score
{
class CommandStack;
class Document;
class DocumentDelegateModel;
class DocumentDelegatePresenter;
struct DocumentContext;

namespace IDocument
{

/**
 * @brief documentFromObject
 * @param obj an object in the application hierarchy
 *
 * @return the Document parent of the object or nullptr.
 */
SCORE_LIB_BASE_EXPORT Document* documentFromObject(const QObject* obj);
SCORE_LIB_BASE_EXPORT Document* documentFromObject(const QObject& obj);
SCORE_LIB_BASE_EXPORT const DocumentContext&
documentContext(const QObject& obj);

/**
 * @brief pathFromDocument
 * @param obj an object in the application hierarchy
 *
 * @return The path between a Document and this object.
 *
 * These functions are not type-safe, hence use them only if there
 * is no other choice (e.g. storing the path to objects of different types)...
 */
SCORE_LIB_BASE_EXPORT ObjectPath unsafe_path(QObject const* const& obj);
SCORE_LIB_BASE_EXPORT ObjectPath unsafe_path(const QObject& obj);

//// Various getters ////

// Presenter of a document plugin.
SCORE_LIB_BASE_EXPORT DocumentDelegatePresenter*
presenterDelegate_generic(const Document& d);

template <typename T>
T* presenterDelegate(const Document& d)
{
  auto pd = presenterDelegate_generic(d);
  if (pd)
    return safe_cast<T*>(pd);
  return nullptr;
}

template <
    typename T,
    std::enable_if_t<
        std::is_base_of<DocumentDelegatePresenter, T>::value>* = nullptr>
T* get(const Document& d)
{
  return presenterDelegate<T>(d);
}

template <typename T>
T* try_presenterDelegate(const Document& d)
{
  return dynamic_cast<T*>(presenterDelegate_generic(d));
}

template <
    typename T,
    std::enable_if_t<
        std::is_base_of<DocumentDelegatePresenter, T>::value>* = nullptr>
T* try_get(const Document& d)
{
  return try_presenterDelegate<T>(d);
}

// Model of a document plugin
// First if we are sure
SCORE_LIB_BASE_EXPORT DocumentDelegateModel&
modelDelegate_generic(const Document& d);

template <typename T>
T& modelDelegate(const Document& d)
{
  return safe_cast<T&>(modelDelegate_generic(d));
}

template <
    typename T,
    std::enable_if_t<
        std::is_base_of<DocumentDelegateModel, T>::value>* = nullptr>
T& get(const Document& d)
{
  return modelDelegate<T>(d);
}

// And then if we are not

template <typename T>
T* try_modelDelegate(const Document& d)
{
  return dynamic_cast<T*>(&modelDelegate_generic(d));
}

template <
    typename T,
    std::enable_if_t<
        std::is_base_of<DocumentDelegateModel, T>::value>* = nullptr>
T* try_get(const Document& d)
{
  return try_modelDelegate<T>(d);
}
}
}
