#include "LibraryPanelDelegate.hpp"
#include <Library/JSONLibrary/LibraryWidget.hpp>
#include <Process/ProcessList.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <QTabWidget>

namespace Library
{
PanelDelegate::PanelDelegate(const iscore::ApplicationContext& ctx):
    iscore::PanelDelegate{ctx},
    m_widget{new QTabWidget}
{
    auto projectModel = new JSONModel;
    auto projectLib = new LibraryWidget{projectModel, m_widget};
    m_widget->addTab(projectLib, QObject::tr("Project"));

    auto systemModel = new JSONModel;
    auto& procs = ctx.components.factory<Process::ProcessFactoryList>();
    for(Process::ProcessModelFactory& proc : procs)
    {
        LibraryElement e;
        e.category = Category::Process;
        e.name = proc.prettyName();
        e.obj["Type"] = "Process";
        e.obj["uuid"] =  toJsonValue(proc.concreteFactoryKey().impl());
        systemModel->addElement(e);

    }

    auto systemLib = new LibraryWidget{systemModel, m_widget};
    m_widget->addTab(systemLib, QObject::tr("System"));

    m_widget->setObjectName("LibraryExplorer");
}

QWidget*PanelDelegate::widget()
{
    return m_widget;
}

const iscore::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
    static const iscore::PanelStatus status{
        false,
        Qt::RightDockWidgetArea,
                0,
                QObject::tr("Library"),
                QObject::tr("Ctrl+Shift+L")};

    return status;
}

}
