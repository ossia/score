#include "PluginControlInterface.hpp"

#include <QApplication>

using namespace iscore;


PluginControlInterface::PluginControlInterface(iscore::Presenter* presenter,
                                               const QString& name,
                                               QObject* parent):
    NamedObject{name, parent},
    m_presenter{presenter}
{
    connect(this, &PluginControlInterface::documentChanged,
            this, &PluginControlInterface::on_documentChanged);

    connect(qApp, &QApplication::applicationStateChanged,
            this, &PluginControlInterface::on_focusChanged);
}

void PluginControlInterface::populateMenus(MenubarManager*)
{

}


QList<OrderedToolbar> PluginControlInterface::makeToolbars()
{
    return {};
}


DocumentDelegatePluginModel*PluginControlInterface::loadDocumentPlugin(
        const QString& name,
        const VisitorVariant& var,
        DocumentModel* parent)
{
    return nullptr;
}


Presenter*PluginControlInterface::presenter() const
{
    return m_presenter;
}

SerializableCommand*PluginControlInterface::instantiateUndoCommand(
        const QString&,
        const QByteArray&)
{
    return nullptr;
}


Document*PluginControlInterface::currentDocument() const
{
    return m_presenter->currentDocument();
}


void PluginControlInterface::on_documentChanged()
{

}

void PluginControlInterface::on_newDocument(iscore::Document* doc)
{

}

void PluginControlInterface::on_loadedDocument(iscore::Document *doc)
{

}


void PluginControlInterface::on_focusChanged(Qt::ApplicationState st)
{
    if(st == Qt::ApplicationActive)
        emit focused();
    else
        emit defocused();
}
