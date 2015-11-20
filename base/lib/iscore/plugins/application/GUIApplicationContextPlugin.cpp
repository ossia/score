#include "GUIApplicationContextPlugin.hpp"
#include <core/application/Application.hpp>

using namespace iscore;


GUIApplicationContextPlugin::GUIApplicationContextPlugin(iscore::Application& app,
                                               const QString& name,
                                               QObject* parent):
    NamedObject{name, parent},
    m_appContext{app}
{
    connect(this, &GUIApplicationContextPlugin::documentChanged,
            this, &GUIApplicationContextPlugin::on_documentChanged);

    connect(qApp, &QApplication::applicationStateChanged,
            this, &GUIApplicationContextPlugin::on_focusChanged);

}

GUIApplicationContextPlugin::~GUIApplicationContextPlugin()
{

}

void GUIApplicationContextPlugin::populateMenus(MenubarManager*)
{

}


std::vector<OrderedToolbar> GUIApplicationContextPlugin::makeToolbars()
{
    return {};
}

std::vector<QAction*> GUIApplicationContextPlugin::actions()
{
    return {};
}


DocumentDelegatePluginModel*GUIApplicationContextPlugin::loadDocumentPlugin(
        const QString& name,
        const VisitorVariant& var,
        Document* parent)
{
    return nullptr;
}

const ApplicationContext& GUIApplicationContextPlugin::context() const
{
    return m_appContext;
}

Document*GUIApplicationContextPlugin::currentDocument() const
{
    return m_appContext.app.presenter().documentManager().currentDocument();
}

void GUIApplicationContextPlugin::prepareNewDocument()
{

}


void GUIApplicationContextPlugin::on_documentChanged(
        iscore::Document* olddoc,
        iscore::Document* newdoc)
{

}

void GUIApplicationContextPlugin::on_newDocument(iscore::Document* doc)
{

}

void GUIApplicationContextPlugin::on_loadedDocument(iscore::Document *doc)
{

}


void GUIApplicationContextPlugin::on_focusChanged(Qt::ApplicationState st)
{
    if(st == Qt::ApplicationActive)
        emit focused();
    else
        emit defocused();
}
