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
    using namespace std;
    QDockWidget* dial = new QDockWidget {v->objectName(), this};
    dial->setWidget(v->getWidget());

    emit insertActionIntoMenubar({MenuInterface::name(ToplevelMenuElement::ViewMenu) + "/" +
                                  MenuInterface::name(ViewMenuElement::Windows),
                                  dial->toggleViewAction()});

    auto dock = v->defaultDock();
    this->addDockWidget(dock, dial);
    if(dock == Qt::LeftDockWidgetArea)
    {
        m_leftWidgets.push_back({v, dial});
        if(m_leftWidgets.size() > 1)
        {
            // Find the one with the biggest priority
            auto it = max_element(begin(m_leftWidgets),
                                  end(m_leftWidgets),
                                  [] (const auto& lhs, const auto& rhs)
            { return lhs.first->priority() < rhs.first->priority(); });

            tabifyDockWidget(it->second, dial);
            it->second->raise();
        }
    }
    else if(dock == Qt::RightDockWidgetArea)
    {
        m_rightWidgets.push_back({v, dial});

        if(m_rightWidgets.size() > 1)
        {
            // Find the one with the biggest priority
            auto it = max_element(begin(m_rightWidgets),
                                  end(m_rightWidgets),
                                  [] (const auto& lhs, const auto& rhs)
            { return lhs.first->priority() < rhs.first->priority(); });
            tabifyDockWidget(it->second, dial);
            it->second->raise();
        }
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
