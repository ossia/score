#include "GfxApplicationPlugin.hpp"

#include <Gfx/Graph/ScreenPlacement.hpp>

#include <score/gfx/DisplayConfig.hpp>

#include <QApplication>
#include <QShortcut>

#include <Execution/DocumentPlugin.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

namespace Gfx
{

DocumentPlugin::DocumentPlugin(const score::DocumentContext& ctx, QObject* parent)
    : score::DocumentPlugin{ctx, "Gfx::DocumentPlugin", parent}
    , context{ctx}
{
  auto& exec_plug = ctx.plugin<Execution::DocumentPlugin>();
  exec_plug.registerAction(exec);
}

DocumentPlugin::~DocumentPlugin() { }

ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : GUIApplicationPlugin{app}
{
  // Early: the canvas watchers have to be in place before a context can be lost.
  installEditorEscapeHatch();
}

void ApplicationPlugin::installEditorEscapeHatch()
{
  // A machine with no window manager can end up showing a render output and
  // nothing else -- by configuration, or because a display setting was wrong
  // and there is no desktop to fix it from. Without a way back the only remedy
  // is a keyboard on another machine, and on an appliance there may not be one.
  if(!score::gfx::oneWindowPerScreen())
    return;

  auto* shortcut = new QShortcut{
      QKeySequence{Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_E}, qApp};
  shortcut->setContext(Qt::ApplicationShortcut);
  QObject::connect(
      shortcut, &QShortcut::activated, qApp, [] { score::gfx::restartIntoEditor(); });
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  doc.model().addPluginModel(new DocumentPlugin{doc.context(), &doc.model()});
}

}
