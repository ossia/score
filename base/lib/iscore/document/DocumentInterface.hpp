#pragma once
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/ModelPath.hpp>

template<typename T>
using remove_qualifs_t = std::decay_t<std::remove_pointer_t<std::decay_t<T>>>;
namespace iscore
{
    class Document;
    class DocumentDelegatePresenterInterface;
    class DocumentDelegateModelInterface;
    class CommandStack;
    class SelectionStack;
    class PanelModel;
    class ObjectLocker;

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

        iscore::CommandStack& commandStack(const QObject& obj);
        iscore::SelectionStack& selectionStack(const QObject& obj);
        iscore::ObjectLocker& locker(const QObject& obj);


        /**
        * @brief pathFromDocument
        * @param obj an object in the application hierarchy
        *
        * @return The path between a Document and this object.
        */
        ObjectPath unsafe_path(QObject const * const& obj);
        ObjectPath unsafe_path(const QObject& obj);

        template<typename T>
        ModelPath<T> safe_path(const T& obj)
        {
            return unsafe_path(static_cast<const QObject&>(obj));
        }




        //// Various getters ////
        // Panel models
        const QList<PanelModel*>& panels(const Document* d);

        // Presenter of a document plugin.
        DocumentDelegatePresenterInterface& presenterDelegate_generic(const Document& d);

        template<typename T> T& presenterDelegate(const Document& d)
        {
            return static_cast<T&>(presenterDelegate_generic(d));
        }

        template<typename T,
                 std::enable_if_t<std::is_base_of<DocumentDelegatePresenterInterface, T>::value>* = nullptr>
        T& get(const Document& d)
        {
            return presenterDelegate<T> (d);
        }


        // Model of a document plugin
        DocumentDelegateModelInterface& modelDelegate_generic(const Document& d);

        template<typename T> T& modelDelegate(const Document& d)
        {
            return static_cast<T&>(modelDelegate_generic(d));
        }

        template<typename T,
                 std::enable_if_t<std::is_base_of<DocumentDelegateModelInterface, T>::value>* = nullptr>
        T& get(const Document& d)
        {
            return modelDelegate<T> (d);
        }
    }
}
