#include "LibraryPanelView.hpp"
#include <core/view/View.hpp>
#include "Plugin/JSONLibrary/LibraryWidget.hpp"
// Library shall have :
// - A global pane (system-wide)
// - A local pane (document-wide, saved in the panel)
LibraryPanelView::LibraryPanelView(iscore::View* parent):
    iscore::PanelView {parent},
    m_widget{new QTabWidget}
{
    auto projectLib = new LibraryWidget{m_widget};
    auto thelib = new JSONLibrary;
    thelib->elements.append(LibraryElement{"dada", {"da di do", "yada"}, {}});
    projectLib->setLibrary(thelib);
    m_widget->addTab(projectLib, tr("Project"));

    auto systemLib = new LibraryWidget{m_widget};
    m_widget->addTab(systemLib, tr("System"));
}

QWidget* LibraryPanelView::getWidget()
{
    return m_widget;
}

Qt::DockWidgetArea LibraryPanelView::defaultDock() const
{
    return Qt::RightDockWidgetArea;
}

int LibraryPanelView::priority() const
{
    return 0;
}

QString LibraryPanelView::prettyName() const
{
    return tr("Library");
}
