#include <Process/ProjectConsolidation.hpp>
#include <Process/ProjectConsolidationDialog.hpp>
#include <Process/ProjectFilesApplicationPlugin.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/actions/MenuManager.hpp>
#include <score/widgets/HelpInteraction.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QAction>
#include <QFileInfo>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>

SCORE_DECLARE_ACTION(
    ConsolidateProject, "&Consolidate project...", Common, QKeySequence::UnknownKey)

namespace Process
{

ProjectFilesApplicationPlugin::ProjectFilesApplicationPlugin(
    const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
}

ProjectFilesApplicationPlugin::~ProjectFilesApplicationPlugin() = default;

void ProjectFilesApplicationPlugin::consolidate()
{
  auto doc = context.docManager.currentDocument();
  if(!doc)
    return;

  // Every path we are about to write is relative to the document's own
  // folder, so there has to be one. This is the "save your set into its own
  // Project folder first" rule that every DAW ends up teaching its users; ask
  // for it up front rather than silently collecting into the wrong place.
  if(doc->metadata().projectFolder().isEmpty())
  {
    const auto res = QMessageBox::question(
        context.mainWindow, QObject::tr("Save the document first"),
        QObject::tr("Consolidating copies the media next to the document, so the "
                    "document has to be saved somewhere first.\n\n"
                    "Save it now?"),
        QMessageBox::Save | QMessageBox::Cancel);
    if(res != QMessageBox::Save)
      return;
    if(!context.docManager.saveDocumentAs(*doc))
      return;
    if(doc->metadata().projectFolder().isEmpty())
      return;
  }

  ProjectConsolidationDialog dialog{doc->context(), context.mainWindow};
  if(dialog.exec() != QDialog::Accepted)
    return;

  // Save right away: the copies on disk and the rewritten references only
  // make sense together, and a document closed without saving would leave
  // the collected files orphaned.
  context.docManager.saveDocument(*doc);
}

void ProjectFilesApplicationPlugin::on_documentSaveAs(
    score::Document& doc, const QString& newFileName)
{
  const QString before = doc.metadata().projectFolder();
  if(before.isEmpty())
    return; // never saved: nothing was relative to anything yet

  const QString after = QFileInfo{newFileName}.absolutePath();
  if(before == after || QFileInfo{after}.canonicalFilePath() == before)
    return; // same folder: the relative references still mean what they meant

  const auto& ctx = doc.context();
  const int relative = countProjectRelativeFiles(ctx);
  if(relative == 0)
    return;

  // Saving elsewhere silently turns every "<PROJECT>:..." into a file that is
  // not there. Neither outcome is obviously right, so ask; but never leave the
  // document in the broken third state.
  // Without a window to ask in (scripted saves), re-anchor: copying gigabytes
  // of media is not something to do behind a script's back, and the document
  // still works afterwards.
  auto choice = QMessageBox::No;
  if(context.mainWindow)
  {
    choice = QMessageBox::StandardButton(QMessageBox::question(
        context.mainWindow, QObject::tr("Media of this project"),
        QObject::tr("This project is being saved to a different folder, and %1 of "
                    "its file(s) live next to the previous one.\n\n"
                    "Copy them into the new folder?\n\n"
                    "Choosing No keeps them where they are and points the project "
                    "at them with absolute paths.")
            .arg(relative),
        QMessageBox::Yes | QMessageBox::No));
  }

  if(choice == QMessageBox::Yes)
    consolidateProjectFiles(ctx, {}, after);
  else
    reanchorProjectFiles(ctx);
}

score::GUIElements ProjectFilesApplicationPlugin::makeGUIElements()
{
  score::GUIElements e;
  if(!context.mainWindow)
    return e;

  m_action = new QAction{context.mainWindow};
  score::setHelp(
      m_action,
      QObject::tr("Copy every file the document uses next to it, and make the "
                  "references relative to the project."));
  connect(
      m_action, &QAction::triggered, this,
      &ProjectFilesApplicationPlugin::consolidate);

  auto& file = context.menus.get().at(score::Menus::File());
  file.menu()->addAction(m_action);

  e.actions.add<Actions::ConsolidateProject>(m_action);
  context.actions.condition<score::EnableActionIfDocument>()
      .add<Actions::ConsolidateProject>();

  return e;
}
}
