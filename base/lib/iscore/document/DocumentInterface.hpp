#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/ObjectPath.hpp>
//#include <iscore/tools/ModelPath.hpp>

namespace iscore
{
class Document;
class DocumentDelegatePresenterInterface;
class DocumentDelegateModelInterface;
class CommandStack;
class SelectionStack;
class PanelModel;
class ObjectLocker;
struct DocumentContext;
namespace IDocument
{
/**
 * @brief documentFromObject
 * @param obj an object in the application hierarchy
 *
 * @return the Document parent of the object or nullptr.
 */
Document* documentFromObject(const QObject* obj);
Document* documentFromObject(const QObject& obj);
DocumentContext& documentContext(const QObject& obj);

iscore::CommandStack& commandStack(const QObject& obj);
iscore::SelectionStack& selectionStack(const QObject& obj);
iscore::ObjectLocker& locker(const QObject& obj);


/**
 * @brief pathFromDocument
 * @param obj an object in the application hierarchy
 *
 * @return The path between a Document and this object.
 *
 * These functions are not type-safe, hence use them only if there
 * is no other choice (e.g. storing the path to objects of different types)...
 */
ObjectPath unsafe_path(QObject const * const& obj);
ObjectPath unsafe_path(const QObject& obj);

//// Various getters ////
// Panel models
const QList<PanelModel*>& panels(const Document* d);

// Presenter of a document plugin.
DocumentDelegatePresenterInterface& presenterDelegate_generic(const Document& d);

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
DocumentDelegateModelInterface& modelDelegate_generic(const Document& d);

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
DocumentDelegateModelInterface* try_modelDelegate_generic(const Document& d);

template<typename T> T* try_modelDelegate(const Document& d)
{
    if(auto md = try_modelDelegate_generic(d))
    {
        return dynamic_cast<T*>(md);
    }
    return nullptr;
}

template<typename T,
         std::enable_if_t<std::is_base_of<DocumentDelegateModelInterface, T>::value>* = nullptr>
T* try_get(const Document& d)
{
    return try_modelDelegate<T> (d);
}
}
}
