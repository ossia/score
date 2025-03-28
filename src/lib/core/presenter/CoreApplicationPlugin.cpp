// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CoreApplicationPlugin.hpp"

#include "AboutDialog.hpp"

#include <score/actions/Menu.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/widgets/HelpInteraction.hpp>

#include <core/presenter/CoreActions.hpp>
#include <core/settings/Settings.hpp>
#include <core/settings/SettingsView.hpp>
#include <core/view/QRecentFilesMenu.h>
#include <core/view/Window.hpp>

#include <QDesktopServices>
#include <QUrl>

SCORE_DECLARE_ACTION(Website, "&Website", Common, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(Documentation, "&Documentation", Common, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(Issues, "&Report Issues", Common, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(Forum, "&Forum", Common, QKeySequence::UnknownKey)

namespace score
{

CoreApplicationPlugin::CoreApplicationPlugin(
    const GUIApplicationContext& app, Presenter& pres)
    : GUIApplicationPlugin{app}
    , m_presenter{pres}
{
}

void CoreApplicationPlugin::newDocument()
{
  m_presenter.m_docManager.newDocument(
      context, getStrongId(m_presenter.m_docManager.documents()),
      *context.interfaces<score::DocumentDelegateList>().begin());
}

void CoreApplicationPlugin::load()
{
  m_presenter.m_docManager.loadFile(context);
}

void CoreApplicationPlugin::save()
{
  if(auto doc = m_presenter.m_docManager.currentDocument())
    m_presenter.m_docManager.saveDocument(*doc);
}

void CoreApplicationPlugin::saveAs()
{
  if(auto doc = m_presenter.m_docManager.currentDocument())
    m_presenter.m_docManager.saveDocumentAs(*doc);
}

void CoreApplicationPlugin::close()
{
  if(auto doc = m_presenter.m_docManager.currentDocument())
    m_presenter.m_docManager.closeDocument(context, *doc);
}

void CoreApplicationPlugin::quit()
{
  if(m_presenter.m_view)
    m_presenter.m_view->close();
}

void CoreApplicationPlugin::restoreLayout()
{
  if(m_presenter.m_view)
    m_presenter.m_view->restoreLayout();
}

void CoreApplicationPlugin::openSettings()
{
  m_presenter.m_settings.view().exec();
}
void CoreApplicationPlugin::openProjectSettings()
{
  /* see https://github.com/ossia/score/issues/1025
  auto doc = m_presenter.documentManager().currentDocument();
  if (doc)
  {
    m_presenter.m_projectSettings.setup(doc->context());
    m_presenter.m_projectSettings.view().exec();
  }
  */
}

void CoreApplicationPlugin::help()
{
  QDesktopServices::openUrl(QUrl("https://ossia.io/score-docs"));
}

void CoreApplicationPlugin::about()
{
  AboutDialog aboutDialog;
  aboutDialog.exec();
  aboutDialog.deleteLater();
}

void CoreApplicationPlugin::loadStack()
{
  m_presenter.m_docManager.loadStack(context);
}

void CoreApplicationPlugin::saveStack()
{
  m_presenter.m_docManager.saveStack();
}

GUIElements CoreApplicationPlugin::makeGUIElements()
{
  GUIElements e;
  auto& menus = e.menus;
  auto& actions = e.actions.container;
  (void)actions;

  menus.reserve(10);
  auto file = new QMenu{tr("&File")};
  auto edit = new QMenu{tr("&Edit")};
  auto object = new QMenu{tr("&Object")};
  auto play = new QMenu{tr("&Play")};
  auto view = new QMenu{tr("&View")};
  auto scripts = new QMenu{tr("&Scripts")};
  auto settings = new QMenu{tr("&Settings")};
  auto about = new QMenu{tr("&Help")};
  menus.emplace_back(file, Menus::File(), Menu::is_toplevel{}, 0);
  menus.emplace_back(edit, Menus::Edit(), Menu::is_toplevel{}, 1);
  menus.emplace_back(object, Menus::Object(), Menu::is_toplevel{}, 2);
  menus.emplace_back(play, Menus::Play(), Menu::is_toplevel{}, 3);
  menus.emplace_back(view, Menus::View(), Menu::is_toplevel{}, 5);
  menus.emplace_back(scripts, Menus::Scripts(), Menu::is_toplevel{}, 10);
  menus.emplace_back(settings, Menus::Settings(), Menu::is_toplevel{}, 15);

  // Menus are by default at int_max - 1 so that they will be sorted before
  menus.emplace_back(
      about, Menus::About(), Menu::is_toplevel{}, std::numeric_limits<int>::max());

  auto export_menu = new QMenu{tr("&Export")};
  menus.emplace_back(export_menu, Menus::Export());

  auto windows_menu = new QMenu{tr("&Windows")};
  menus.emplace_back(windows_menu, Menus::Windows());

  ////// File //////
  // New
  // ---
  // Load
  // Recent
  // Save
  // Save As
  // ---
  // Export
  // ---
  // Close
  // Quit

  if(m_presenter.view())
  {

    {
      auto new_doc = new QAction(m_presenter.view());
      connect(new_doc, &QAction::triggered, this, &CoreApplicationPlugin::newDocument);
      file->addAction(new_doc);
      e.actions.add<Actions::New>(new_doc);
    }

    file->addSeparator();

    {
      auto load_doc = new QAction(m_presenter.view());
      connect(load_doc, &QAction::triggered, this, &CoreApplicationPlugin::load);
      e.actions.add<Actions::Load>(load_doc);
      file->addAction(load_doc);
    }

    file->addMenu(m_presenter.m_docManager.recentFiles());

    auto& cond = context.actions.condition<EnableActionIfDocument>();
    {
      auto save_doc = new QAction(m_presenter.view());
      save_doc->setDisabled(true);
      connect(save_doc, &QAction::triggered, this, &CoreApplicationPlugin::save);
      e.actions.add<Actions::Save>(save_doc);
      cond.add<Actions::Save>();
      file->addAction(save_doc);
    }

    {
      auto saveas_doc = new QAction(m_presenter.view());
      saveas_doc->setDisabled(true);
      connect(saveas_doc, &QAction::triggered, this, &CoreApplicationPlugin::saveAs);
      e.actions.add<Actions::SaveAs>(saveas_doc);
      cond.add<Actions::SaveAs>();
      file->addAction(saveas_doc);
    }

#ifdef SCORE_DEBUG
    file->addSeparator();
    file->addMenu(export_menu);
    // Add command stack import / export
    {
      auto loadStack_act = new QAction(m_presenter.view());
      connect(
          loadStack_act, &QAction::triggered, this, &CoreApplicationPlugin::loadStack);
      actions.emplace_back(
          loadStack_act, tr("&Load a stack"), "LoadStack", "Common",
          QKeySequence::UnknownKey);
      export_menu->addAction(loadStack_act);
    }

    {
      auto saveStack_act = new QAction(m_presenter.view());
      connect(
          saveStack_act, &QAction::triggered, this, &CoreApplicationPlugin::saveStack);
      actions.emplace_back(
          saveStack_act, tr("&Save a stack"), "SaveStack", "Common",
          QKeySequence::UnknownKey);
      export_menu->addAction(saveStack_act);
    }
#endif

    file->addSeparator();

    {
      auto close_act = new QAction(m_presenter.view());
      close_act->setDisabled(true);
      connect(close_act, &QAction::triggered, this, &CoreApplicationPlugin::close);
      e.actions.add<Actions::Close>(close_act);
      cond.add<Actions::Close>();
      file->addAction(close_act);
    }

    {
      auto quit_act = new QAction(m_presenter.view());
      connect(quit_act, &QAction::triggered, this, &CoreApplicationPlugin::quit);
      e.actions.add<Actions::Quit>(quit_act);
      file->addAction(quit_act);
    }

    ////// View //////
    view->addMenu(windows_menu);
    {
      auto act = new QAction(m_presenter.view());
      connect(act, &QAction::triggered, this, &CoreApplicationPlugin::restoreLayout);
      e.actions.add<Actions::RestoreLayout>(act);
      windows_menu->addAction(act);
    }

    ////// Settings //////
    {
      auto settings_act = new QAction(m_presenter.view());
      connect(
          settings_act, &QAction::triggered, this, &CoreApplicationPlugin::openSettings);
      e.actions.add<Actions::OpenSettings>(settings_act);
      settings->addAction(settings_act);
    }
    /* see https://github.com/ossia/score/issues/1025
    {
      auto settings_act = new QAction(m_presenter.view());
      connect(
          settings_act, &QAction::triggered, this, &CoreApplicationPlugin::openProjectSettings);
      e.actions.add<Actions::OpenProjectSettings>(settings_act);
      settings->addAction(settings_act);
    }
    */

    ////// Scripts /////
    {
      auto script_act
          = new QAction(tr(".mjs scripts will go there"), m_presenter.view());
      script_act->setObjectName("DefaultScriptMenuAction");
      script_act->setEnabled(false);
      scripts->addAction(script_act);
    }

    ////// About /////
    {
      auto about_act = new QAction(m_presenter.view());
      score::setHelp(about_act, tr("About ossia score"));
      connect(about_act, &QAction::triggered, this, &CoreApplicationPlugin::about);
      e.actions.add<Actions::About>(about_act);
      about->addAction(about_act);
    }

    {
      auto website_act = new QAction(m_presenter.view());
      score::setHelp(website_act, tr("Open link to ossia website https://ossia.io/"));
      connect(website_act, &QAction::triggered, this, [] {
        QDesktopServices::openUrl(QUrl("https://ossia.io/"));
      });
      e.actions.add<Actions::Website>(website_act);
      about->addAction(website_act);
    }

    {
      auto doc_act = new QAction(m_presenter.view());
      score::setHelp(
          doc_act, tr("Open link to documentation https://ossia.io/score-docs"));
      connect(doc_act, &QAction::triggered, this, &CoreApplicationPlugin::help);
      e.actions.add<Actions::Documentation>(doc_act);
      about->addAction(doc_act);
    }

    {
      auto issues_act = new QAction(m_presenter.view());
      score::setHelp(
          issues_act, tr("Report issues on the Github ossia score repository"));
      connect(issues_act, &QAction::triggered, this, [] {
        QDesktopServices::openUrl(QUrl("https://github.com/ossia/score/issues"));
      });
      e.actions.add<Actions::Issues>(issues_act);
      about->addAction(issues_act);
    }

    {
      auto forum_act = new QAction(m_presenter.view());
      score::setHelp(forum_act, tr("Open link to ossia forum https://forum.ossia.io"));
      connect(forum_act, &QAction::triggered, this, [] {
        QDesktopServices::openUrl(QUrl("https://forum.ossia.io"));
      });
      e.actions.add<Actions::Forum>(forum_act);
      about->addAction(forum_act);
    }
  }

  return e;
}
}
