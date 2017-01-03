
#include <QApplication>

#include "GUIApplicationPlugin.hpp"
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/Presenter.hpp>

class QAction;
class QObject;
namespace iscore
{
class Document;
} // namespace iscore
struct VisitorVariant;

namespace iscore
{

GUIApplicationPlugin::GUIApplicationPlugin(
    const iscore::GUIApplicationContext& app)
    : context{app}
{
}

GUIApplicationPlugin::~GUIApplicationPlugin() = default;

void GUIApplicationPlugin::initialize()
{
}

GUIElements GUIApplicationPlugin::makeGUIElements()
{
  return {};
}

Document* GUIApplicationPlugin::currentDocument() const
{
  return context.documents.currentDocument();
}

bool GUIApplicationPlugin::handleStartup()
{
  return false;
}

void GUIApplicationPlugin::prepareNewDocument()
{
}

void GUIApplicationPlugin::on_documentChanged(
    iscore::Document* olddoc, iscore::Document* newdoc)
{
}

void GUIApplicationPlugin::on_activeWindowChanged()
{
}

void GUIApplicationPlugin::on_initDocument(iscore::Document& doc)
{
}

void GUIApplicationPlugin::on_newDocument(iscore::Document& doc)
{
}

void GUIApplicationPlugin::on_loadedDocument(iscore::Document& doc)
{
}

void GUIApplicationPlugin::on_createdDocument(iscore::Document& doc)
{
}
}
