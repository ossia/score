// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "GUIApplicationPlugin.hpp"

#include <QApplication>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/Presenter.hpp>

class QAction;
class QObject;
namespace score
{
class Document;
} // namespace score
struct VisitorVariant;

namespace score
{

ApplicationPlugin::~ApplicationPlugin() = default;

ApplicationPlugin::ApplicationPlugin(const ApplicationContext& ctx)
    : context{ctx}
{
}

void ApplicationPlugin::initialize()
{
}

GUIApplicationPlugin::GUIApplicationPlugin(
    const score::GUIApplicationContext& app)
    : context{app}
{
}

GUIApplicationPlugin::~GUIApplicationPlugin() = default;

GUIElements GUIApplicationPlugin::makeGUIElements()
{
  return {};
}

void GUIApplicationPlugin::initialize()
{
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
    score::Document* olddoc, score::Document* newdoc)
{
}

void GUIApplicationPlugin::on_activeWindowChanged()
{
}

void GUIApplicationPlugin::on_initDocument(score::Document& doc)
{
}

void GUIApplicationPlugin::on_newDocument(score::Document& doc)
{
}

void GUIApplicationPlugin::on_loadedDocument(score::Document& doc)
{
}

void GUIApplicationPlugin::on_createdDocument(score::Document& doc)
{
}
}
