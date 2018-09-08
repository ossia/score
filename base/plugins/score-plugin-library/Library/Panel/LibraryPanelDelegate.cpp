// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LibraryPanelDelegate.hpp"

#include <Library/JSONLibrary/LibraryWidget.hpp>
#include <Library/JSONLibrary/ProcessesItemModel.hpp>
#include <Process/ProcessList.hpp>
#include <QTabWidget>
#include <score/application/GUIApplicationContext.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

namespace Library
{
PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}, m_widget{new QTabWidget}
{
  auto projectModel = new JSONModel;
  auto projectLib = new LibraryWidget{projectModel, m_widget};
  m_widget->addTab(projectLib, QObject::tr("Project"));

  auto systemModel = new JSONModel;
  auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
  for (Process::ProcessModelFactory& proc : procs)
  {
    LibraryElement e;
    e.category = Category::Process;
    e.name = proc.prettyName();
    e.obj["Type"] = "Process";
    e.obj["uuid"] = toJsonValue(proc.concreteKey().impl());
    systemModel->addElement(e);
  }

  auto systemLib = new LibraryWidget{systemModel, m_widget};
  m_widget->addTab(systemLib, QObject::tr("System"));

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
  static const score::PanelStatus status{false, Qt::RightDockWidgetArea, 0,
                                         QObject::tr("Library"),
                                         QObject::tr("Ctrl+Shift+B")};

  return status;
}
}
