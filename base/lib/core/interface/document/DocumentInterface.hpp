#pragma once
#include <tools/ObjectPath.hpp>

namespace iscore
{
    class Document;
    class DocumentDelegatePresenterInterface;
    class DocumentDelegateModelInterface;

    namespace IDocument
    {
        /**
        * @brief documentFromObject
        * @param obj an object in the application hierarchy
        *
        * @return the Document parent of the object or nullptr.
        */
        Document* documentFromObject(QObject* obj);

        /**
        * @brief pathFromDocument
        * @param obj an object in the application hierarchy
        *
        * @return The path between a Document and this object.
        */
        ObjectPath path(QObject* obj);

        DocumentDelegatePresenterInterface& presenterDelegate_generic(const Document* d);

        template<typename T> T& presenterDelegate(const Document* d)
        {
            return static_cast<T&>(presenterDelegate_generic(d));
        }


        DocumentDelegateModelInterface& modelDelegate_generic(const Document* d);

        template<typename T> T& modelDelegate(const Document* d)
        {
            return static_cast<T&>(presenterDelegate_generic(d));
        }

//		template<typename T>
//		T& get(const Document* d);

        template<typename T,
                 typename std::enable_if<std::is_base_of<DocumentDelegatePresenterInterface, T>::value>::type* = nullptr>
        T& get(const Document* d)
        {
            return presenterDelegate<T> (d);
        }

        template<typename T,
                 typename std::enable_if<std::is_base_of<DocumentDelegateModelInterface, T>::value>::type* = nullptr>
        T& get(const Document* d)
        {
            return modelDelegate<T> (d);
        }

    }

}
