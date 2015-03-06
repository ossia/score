#include "DocumentInterface.hpp"
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>

#include <interface/panel/PanelModelInterface.hpp>

iscore::Document* iscore::IDocument::documentFromObject(const QObject* obj)
{
    QString objName {obj ? obj->objectName() : "INVALID"};

    while(obj && obj->objectName() != "Document")
    {
        obj = obj->parent();
    }

    if(!obj)
        throw std::runtime_error(
            QString("Object (name: %1) is not part of a Document!")
            .arg(objName)
            .toStdString());

    return static_cast<Document*>(const_cast<QObject*>(obj));
}


ObjectPath iscore::IDocument::path(const QObject* obj)
{
    return ObjectPath::pathBetweenObjects(documentFromObject(obj), obj);
}


iscore::DocumentDelegatePresenterInterface& iscore::IDocument::presenterDelegate_generic(const iscore::Document* d)
{
    return *d->presenter()->presenterDelegate();
}


iscore::DocumentDelegateModelInterface& iscore::IDocument::modelDelegate_generic(const iscore::Document* d)
{
    return *d->model()->modelDelegate();
}


const QList<iscore::PanelModelInterface*>& iscore::IDocument::panels(const iscore::Document* d)
{
    return d->model()->panels();
}


iscore::PanelModelInterface* iscore::IDocument::panel(const QString& name, const iscore::Document* d)
{
    using namespace std;
    auto panels = d->model()->panels();

    auto it = find_if(begin(panels), end(panels),
                      [name](PanelModelInterface* panel)
    {
              return panel->objectName() == name;
    });

    return (it != end(panels) ? *it : nullptr);
}


iscore::CommandStack* iscore::IDocument::commandStack(const iscore::Document* d)
{
    return d->presenter()->commandStack();
}


iscore::SelectionStack &iscore::IDocument::selectionStack(const iscore::Document *d)
{
    return d->presenter()->selectionStack();
}
