
#include <QApplication>

#include "GUIApplicationContextPlugin.hpp"
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/document/Document.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/widgets/OrderedToolbar.hpp>

class QAction;
class QObject;
namespace iscore {
class Document;
}  // namespace iscore
struct VisitorVariant;



namespace iscore
{

GUIApplicationContextPlugin::GUIApplicationContextPlugin(
        const iscore::ApplicationContext& app,
        const QString& name,
        QObject* parent):
    context{app}
{

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

Document*GUIApplicationContextPlugin::currentDocument() const
{
    return context.documents.currentDocument();
}

bool GUIApplicationContextPlugin::handleStartup()
{
    return false;
}

void GUIApplicationContextPlugin::prepareNewDocument()
{

}


void GUIApplicationContextPlugin::on_documentChanged(
        iscore::Document* olddoc,
        iscore::Document* newdoc)
{

}

void GUIApplicationContextPlugin::on_activeWindowChanged()
{

}

void GUIApplicationContextPlugin::on_newDocument(iscore::Document* doc)
{

}

void GUIApplicationContextPlugin::on_loadedDocument(iscore::Document *doc)
{

}
}
