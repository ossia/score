#pragma once
#include <tools/ObjectPath.hpp>

namespace iscore
{
    class Document;
    class DocumentDelegatePresenterInterface;
    class DocumentDelegateModelInterface;
    class CommandQueue;
    class PanelModelInterface;

    namespace IDocument
    {
        /**
        * @brief documentFromObject
        * @param obj an object in the application hierarchy
        *
        * @return the Document parent of the object or nullptr.
        */
        Document* documentFromObject(const QObject *obj);

        /**
        * @brief pathFromDocument
        * @param obj an object in the application hierarchy
        *
        * @return The path between a Document and this object.
        */
        ObjectPath path(const QObject *obj);

        //// Various getters ////
        // Panel models
        const QList<PanelModelInterface*>& panels(const Document* d);
        PanelModelInterface* panel(const QString& name, const Document* d);

        // Command queue
        CommandQueue* commandQueue(const Document* d);

        // Presenter of a document plugin.
        DocumentDelegatePresenterInterface& presenterDelegate_generic(const Document* d);

        template<typename T> T& presenterDelegate(const Document* d)
        {
            return static_cast<T&>(presenterDelegate_generic(d));
        }

        template<typename T,
                 typename std::enable_if<std::is_base_of<DocumentDelegatePresenterInterface, T>::value>::type* = nullptr>
        T& get(const Document* d)
        {
            return presenterDelegate<T> (d);
        }


        // Model of a document plugin
        DocumentDelegateModelInterface& modelDelegate_generic(const Document* d);

        template<typename T> T& modelDelegate(const Document* d)
        {
            return static_cast<T&>(presenterDelegate_generic(d));
        }

        template<typename T,
                 typename std::enable_if<std::is_base_of<DocumentDelegateModelInterface, T>::value>::type* = nullptr>
        T& get(const Document* d)
        {
            return modelDelegate<T> (d);
        }
    }
}
