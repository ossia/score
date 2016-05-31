
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
        const iscore::GUIApplicationContext& app):
    context{app}
{

}

GUIApplicationContextPlugin::~GUIApplicationContextPlugin() = default;

void GUIApplicationContextPlugin::initialize()
{

}

auto GUIApplicationContextPlugin::makeGUIElements() -> GUIElements
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
