#include "PluginControlInterface.hpp"
#include <core/application/Application.hpp>

using namespace iscore;


PluginControlInterface::PluginControlInterface(iscore::Application& app,
                                               const QString& name,
                                               QObject* parent):
    NamedObject{name, parent},
    m_appContext{app}
{
    connect(this, &PluginControlInterface::documentChanged,
            this, &PluginControlInterface::on_documentChanged);

    connect(qApp, &QApplication::applicationStateChanged,
            this, &PluginControlInterface::on_focusChanged);

}

PluginControlInterface::~PluginControlInterface()
{

}

void PluginControlInterface::populateMenus(MenubarManager*)
{

}


std::vector<OrderedToolbar> PluginControlInterface::makeToolbars()
{
    return {};
}

std::vector<QAction*> PluginControlInterface::actions()
{
    return {};
}


DocumentDelegatePluginModel*PluginControlInterface::loadDocumentPlugin(
        const QString& name,
        const VisitorVariant& var,
        Document* parent)
{
    return nullptr;
}

const ApplicationContext& PluginControlInterface::context() const
{
    return m_appContext;
}

Document*PluginControlInterface::currentDocument() const
{
    return m_appContext.app.presenter().documentManager().currentDocument();
}

void PluginControlInterface::prepareNewDocument()
{

}


void PluginControlInterface::on_documentChanged(
        iscore::Document* olddoc,
        iscore::Document* newdoc)
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
