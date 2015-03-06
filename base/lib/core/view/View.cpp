#include <core/view/View.hpp>
#include <interface/plugincontrol/MenuInterface.hpp>
#include <QDockWidget>
#include <QGridLayout>
#include <QDesktopWidget>

#include <core/application/Application.hpp>
#include <core/document/DocumentView.hpp>

#include <interface/panel/PanelViewInterface.hpp>
#include <interface/panel/PanelPresenterInterface.hpp>

#include <QTabWidget>

using namespace iscore;

View::View(QObject* parent) :
    QMainWindow {},
    m_tabWidget{new QTabWidget}
{
    setObjectName("View");
    setUnifiedTitleAndToolBarOnMac(true);

    this->setDockOptions(QMainWindow::ForceTabbedDocks | QMainWindow::VerticalTabs);

    QDesktopWidget w;
    auto rect = w.availableGeometry();
    this->resize(rect.width() * 0.75, rect.height() * 0.75);

    setCentralWidget(m_tabWidget);
    connect(m_tabWidget, &QTabWidget::currentChanged,
            [&] (int index) {
           auto view = static_cast<DocumentView*>(m_tabWidget->widget(index));
           emit activeDocumentChanged(view->document());
    });
}

void View::addDocumentView(DocumentView* doc)
{
    doc->setParent(this);
    m_tabWidget->addTab(doc, "Document");
}


void View::setupPanelView(PanelViewInterface* v)
{
    addSidePanel(v->getWidget(), v->objectName(), v->defaultDock());
}

void View::addSidePanel(QWidget* widg, QString name, Qt::DockWidgetArea dock)
{
    QDockWidget* dial = new QDockWidget {name, this};
    dial->setWidget(widg);

    emit insertActionIntoMenubar({MenuInterface::name(ToplevelMenuElement::ViewMenu) + "/" +
                                  MenuInterface::name(ViewMenuElement::Windows),
                                  dial->toggleViewAction()});

    this->addDockWidget(dock, dial);
    if(dock == Qt::LeftDockWidgetArea)
    {
        m_leftWidgets.push_back(dial);
        if(m_leftWidgets.size() > 1)
            tabifyDockWidget(m_leftWidgets.first(), dial);
    }
    else if(dock == Qt::RightDockWidgetArea)
    {
        m_rightWidgets.push_back(dial);

        if(m_rightWidgets.size() > 1)
            tabifyDockWidget(m_rightWidgets.first(), dial);
    }
}
