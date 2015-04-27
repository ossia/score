#include <core/view/View.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <QDockWidget>
#include <QGridLayout>
#include <QDesktopWidget>

#include <core/application/Application.hpp>
#include <core/document/DocumentView.hpp>

#include <iscore/plugins/panel/PanelViewInterface.hpp>
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>

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
           if(!view)
               return;
           emit activeDocumentChanged(view->document());
    });


    connect(m_tabWidget, &QTabWidget::tabCloseRequested, [&] (int index)
    {
        emit closeRequested(static_cast<DocumentView*>(m_tabWidget->widget(index))->document());
    });
}

void View::addDocumentView(DocumentView* doc)
{
    doc->setParent(this);
    m_tabWidget->addTab(doc, "Document");
    m_tabWidget->setCurrentIndex(m_tabWidget->count() - 1);
    m_tabWidget->setTabsClosable(true);
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

void View::closeDocument(DocumentView *doc)
{
    for(int i = 0; i < m_tabWidget->count(); i++)
    {
        if(doc == m_tabWidget->widget(i))
        {
            m_tabWidget->removeTab(i);
            return;
        }
    }
}
