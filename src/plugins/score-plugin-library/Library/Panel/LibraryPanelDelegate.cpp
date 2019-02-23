// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LibraryPanelDelegate.hpp"

#include <Library/FileSystemModel.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/LibraryWidget.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/document/Document.hpp>

#include <QTabWidget>
namespace Library
{
PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}, m_widget{new QTabWidget}
{
  m_widget->addTab(
      new SystemLibraryWidget{ctx, m_widget}, QObject::tr("Library"));

  m_projectView = new ProjectLibraryWidget{ctx, m_widget};
  m_widget->addTab(m_projectView, QObject::tr("Project"));

  m_widget->addTab(new ProcessWidget{ctx, m_widget}, QObject::tr("Processes"));

  m_widget->setObjectName("LibraryExplorer");
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{true,
                                         Qt::LeftDockWidgetArea,
                                         0,
                                         QObject::tr("Library"),
                                         QObject::tr("Ctrl+Shift+B")};

  return status;
}

void PanelDelegate::on_modelChanged(
    score::MaybeDocument oldm,
    score::MaybeDocument newm)
{
  if (newm)
  {
    if (auto file = newm->document.metadata().fileName(); QFile::exists(file))
    {
      m_projectView->setRoot(QFileInfo{file}.absolutePath());
      return;
    }
  }

  m_projectView->setRoot({});
}
}
