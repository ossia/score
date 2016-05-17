#include "LibraryPanelFactory.hpp"
#include <Library/JSONLibrary/LibraryWidget.hpp>
#include <QTabWidget>

namespace Library
{
PanelDelegate::PanelDelegate(const iscore::ApplicationContext& ctx):
    iscore::PanelDelegate{ctx},
    m_widget{new QTabWidget}
{
    auto projectLib = new LibraryWidget{m_widget};
    m_widget->addTab(projectLib, QObject::tr("Project"));

    auto systemLib = new LibraryWidget{m_widget};
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
                QObject::tr("Ctrl+L")};

    return status;
}

std::unique_ptr<iscore::PanelDelegate> PanelDelegateFactory::make(
        const iscore::ApplicationContext& ctx)
{
    return std::make_unique<PanelDelegate>(ctx);
}

}
