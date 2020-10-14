// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LibraryPanelDelegate.hpp"

#include <Library/LibrarySettings.hpp>
#include <Library/ProcessWidget.hpp>
#include <Library/ProjectLibraryWidget.hpp>
#include <Library/SystemLibraryWidget.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/document/Document.hpp>

#include <QFileInfo>
#include <QTabWidget>
namespace Library
{
UserPanel::UserPanel(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}, m_widget{new SystemLibraryWidget{ctx, nullptr}}
{
  m_widget->setStatusTip(
      QObject::tr("This panel allows to browse medias and presets in the documents. \n"
                  "Check for library updates on \n"
                  "github.com/ossia/score-user-library"));
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
    : score::PanelDelegate{ctx}, m_widget{new ProjectLibraryWidget{ctx, nullptr}}
{
  m_widget->setStatusTip(
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
  if (newm)
  {
    auto& meta = newm->document.metadata();
    m_widget->setRoot(meta);
    return;
  }

  m_widget->unsetRoot();
}

ProcessPanel::ProcessPanel(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}, m_widget{new ProcessWidget{ctx, nullptr}}
{
  m_widget->setStatusTip(
        QObject::tr("This panel allows to list available processes, effects and plug-ins."));
}

ProcessWidget &ProcessPanel::processWidget() const noexcept
{
  return *(ProcessWidget*)m_widget;
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
