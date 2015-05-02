#include "PluginControlInterface.hpp"
using namespace iscore;


PluginControlInterface::PluginControlInterface(const QString& name, QObject* parent):
    NamedObject{name, parent}
{
    connect(this, &PluginControlInterface::documentChanged,
            this, &PluginControlInterface::on_documentChanged);
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


void PluginControlInterface::setPresenter(Presenter* p)
{
    m_presenter = p;
    on_presenterChanged();
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

void PluginControlInterface::on_presenterChanged()
{

}
