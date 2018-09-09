// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LibraryPanelDelegate.hpp"

#include <Library/JSONLibrary/LibraryWidget.hpp>
#include <Library/JSONLibrary/ProcessesItemModel.hpp>
#include <Library/JSONLibrary/FileSystemModel.hpp>
#include <Library/JSONLibrary/SystemLibraryModel.hpp>
#include <Library/JSONLibrary/ProjectLibraryModel.hpp>
#include <Process/ProcessList.hpp>
#include <QTabWidget>
#include <score/application/GUIApplicationContext.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <Library/LibrarySettings.hpp>
#include <core/document/Document.hpp>
namespace Library
{
PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}, m_widget{new QTabWidget}
{
  {
    m_systemModel = new FileSystemModel{ctx, m_widget};
    auto system_lib = new SystemLibraryWidget{*m_systemModel, m_widget};

    auto idx = m_systemModel->setRootPath(ctx.settings<Library::Settings::Model>().getPath());
    system_lib->tree().setRootIndex(idx);
    for (int i = 1; i < m_systemModel->columnCount(); ++i)
        system_lib->tree().hideColumn(i);
    m_widget->addTab(system_lib, QObject::tr("Library"));
  }

  {
    m_projectModel = new FileSystemModel{ctx, m_widget};
    m_projectView = new ProjectLibraryWidget{*m_projectModel, m_widget};

    m_widget->addTab(m_projectView, QObject::tr("Project"));
  }

  auto proc_model = new ProcessesItemModel{ctx, m_widget};
  auto proc_lib = new ProcessWidget{*proc_model, m_widget};
  m_widget->addTab(proc_lib, QObject::tr("Processes"));

  m_widget->setObjectName("LibraryExplorer");
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{true, Qt::LeftDockWidgetArea, 0,
                                         QObject::tr("Library"),
                                         QObject::tr("Ctrl+Shift+B")};

  return status;
}

void PanelDelegate::on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm)
{
  if(newm)
  {
    if(auto file = newm->document.metadata().fileName(); QFile::exists(file))
    {
      auto idx = m_projectModel->setRootPath(QFileInfo{file}.absolutePath());

      m_projectView->tree().setModel(m_projectModel);
      m_projectView->tree().setRootIndex(idx);
      for (int i = 1; i < m_projectModel->columnCount(); ++i)
          m_projectView->tree().hideColumn(i);
      return;
    }
  }

  m_projectView->tree().setModel(nullptr);
}
}


