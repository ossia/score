#include "DocumentInterface.hpp"
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>

iscore::Document* iscore::IDocument::documentFromObject(const QObject* obj)
{
    QString objName {obj ? obj->objectName() : "INVALID"};

    while(obj && !qobject_cast<const Document*>(obj))
    {
        obj = obj->parent();
    }

    if(!obj)
    {
        qDebug("fail");
        throw std::runtime_error(
            QString("Object (name: %1) is not part of a Document!")
            .arg(objName)
            .toStdString());
    }

    return safe_cast<Document*>(const_cast<QObject*>(obj));
}


iscore::Document* iscore::IDocument::documentFromObject(const QObject& obj)
{
    return documentFromObject(&obj);
}

ObjectPath iscore::IDocument::unsafe_path(QObject const * const&  obj)
{
    return ObjectPath::pathBetweenObjects(&documentFromObject(obj)->model(), obj);
}

ObjectPath iscore::IDocument::unsafe_path(const QObject &obj)
{
    return unsafe_path(&obj);
}

iscore::DocumentDelegatePresenterInterface& iscore::IDocument::presenterDelegate_generic(const iscore::Document& d)
{
    return *d.presenter().presenterDelegate();
}


iscore::DocumentDelegateModelInterface* iscore::IDocument::try_modelDelegate_generic(const Document& d)
{
    return d.model().modelDelegate();
}

iscore::DocumentDelegateModelInterface& iscore::IDocument::modelDelegate_generic(const Document& d)
{
    return *try_modelDelegate_generic(d);
}


const QList<iscore::PanelModel*>& iscore::IDocument::panels(const iscore::Document* d)
{
    return d->model().panels();
}


iscore::CommandStack &iscore::IDocument::commandStack(const QObject &obj)
{
    return documentFromObject(obj)->commandStack();
}

iscore::SelectionStack &iscore::IDocument::selectionStack(const QObject &obj)
{
    return documentFromObject(obj)->selectionStack();
}

iscore::ObjectLocker &iscore::IDocument::locker(const QObject &obj)
{
    return documentFromObject(obj)->locker();
}

iscore::DocumentContext&iscore::IDocument::documentContext(const QObject& obj)
{
    return documentFromObject(obj)->context();
}
