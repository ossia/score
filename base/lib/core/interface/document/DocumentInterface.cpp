#include "DocumentInterface.hpp"
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>

iscore::Document* iscore::IDocument::documentFromObject (QObject* obj)
{
    QString objName {obj ? obj->objectName() : "INVALID"};

    while (obj && obj->objectName() != "Document")
    {
        obj = obj->parent();
    }

    if (!obj)
        throw std::runtime_error (
            QString ("Object (name: %1) is not part of a Document!")
            .arg (objName)
            .toStdString() );

    return static_cast<Document*> (obj);
}


ObjectPath iscore::IDocument::path (QObject* obj)
{
    return ObjectPath::pathBetweenObjects (documentFromObject (obj), obj);
}


iscore::DocumentDelegatePresenterInterface& iscore::IDocument::presenterDelegate_generic (const iscore::Document* d)
{
    return *d->presenter()->presenterDelegate();
}


iscore::DocumentDelegateModelInterface& iscore::IDocument::modelDelegate_generic (const iscore::Document* d)
{
    return *d->model()->modelDelegate();
}
