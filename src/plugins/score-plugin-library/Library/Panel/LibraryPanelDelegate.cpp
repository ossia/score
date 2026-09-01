// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LibraryPanelDelegate.hpp"

#include <Process/ProcessList.hpp>

#include <Library/LibrarySettings.hpp>
#include <Library/ProcessWidget.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <Library/ProjectLibraryWidget.hpp>
#include <Library/SystemLibraryWidget.hpp>
#include <Library/RemoteFileSystemModel.hpp>

#include <QStackedWidget>

#include <score/application/GUIApplicationContext.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/widgets/HelpInteraction.hpp>

#include <core/document/Document.hpp>

#include <QFileInfo>
#include <QTabWidget>
namespace Library
{
UserPanel::UserPanel(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_widget{new QStackedWidget{nullptr}}
    , m_local{new SystemLibraryWidget{ctx, nullptr}}
    , m_remote{new RemoteLibraryWidget{nullptr}}
{
  m_widget->addWidget(m_local);
  m_widget->addWidget(m_remote);

  score::setHelp(m_widget, 
      QObject::tr("This panel allows to browse medias and presets in the documents. \n"
                  "Check for library updates on \n"
                  "github.com/ossia/score-user-library"));
}

void UserPanel::on_modelChanged(score::MaybeDocument, score::MaybeDocument newm)
{
  const bool remote = newm && newm->role() != score::DocumentRole::Local;
  if(!remote)
  {
    m_remote->clear();
    m_widget->setCurrentWidget(m_local);
    return;
  }

  // The user library of the machine the score runs on. Listed through the
  // document's environment, which is what knows where that is.
  m_remote->browse(
      [doc = &newm->document] { return &doc->environment(); },
      score::Uri{score::UriScheme::Library, QString{}});
  m_widget->setCurrentWidget(m_remote);
}

QWidget* UserPanel::widget()
{
  return m_widget;
}

const score::PanelStatus& UserPanel::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      true,
      false,
      Qt::LeftDockWidgetArea,
      40,
      QObject::tr("User Library"),
      "library",
      QObject::tr("Ctrl+Shift+B")};

  return status;
}

ProjectPanel::ProjectPanel(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_widget{new ProjectLibraryWidget{ctx, nullptr}}
{
  score::setHelp(m_widget, 
      QObject::tr("This panel allows to browse the content of the folder of "
                  "the current project."));
}

QWidget* ProjectPanel::widget()
{
  return m_widget;
}

const score::PanelStatus& ProjectPanel::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      true,
      false,
      Qt::LeftDockWidgetArea,
      30,
      QObject::tr("Project folder"),
      "project",
      QObject::tr("Ctrl+Shift+L")};

  return status;
}

void ProjectPanel::on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm)
{
  if(newm)
  {
    auto& meta = newm->document.metadata();
    m_widget->setRoot(meta);
    return;
  }

  m_widget->unsetRoot();
}

ProcessPanel::ProcessPanel(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_widget{new ProcessWidget{ctx, nullptr}}
{
  score::setHelp(m_widget, QObject::tr(
      "This panel allows to list available processes, effects and plug-ins."));
}

ProcessWidget& ProcessPanel::processWidget() const noexcept
{
  return *(ProcessWidget*)m_widget;
}

void ProcessPanel::on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm)
{
  // Only when the list currently describes another machine. rescan() resets a
  // watch that is shared by every model and restarts an asynchronous scan, so
  // doing it on every change raced the scan the constructor had just started --
  // which crashed the process, on a worker thread, some of the time.
  const bool wasRemote = oldm && oldm->role() != score::DocumentRole::Local;
  const bool isLocal = !newm || newm->role() == score::DocumentRole::Local;
  if(!wasRemote || !isLocal)
    return;

  processWidget().processModel().rescan();
}

QWidget* ProcessPanel::widget()
{
  return m_widget;
}

const score::PanelStatus& ProcessPanel::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      true,
      false,
      Qt::LeftDockWidgetArea,
      50,
      QObject::tr("Processes"),
      "process_library",
      QObject::tr("Ctrl+Shift+P")};

  return status;
}

}
