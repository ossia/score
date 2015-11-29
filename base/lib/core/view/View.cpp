#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/plugins/panel/PanelView.hpp>
#include <QAction>
#include <qcoreevent.h>
#include <QDesktopWidget>
#include <QDockWidget>
#include <QEvent>
#include <QFlags>
#include <qnamespace.h>
#include <QRect>
#include <QTabBar>
#include <QTabWidget>
#include <QWidget>
#include <algorithm>
#include <iterator>
#include <set>

class QObject;

using namespace iscore;

View::View(QObject* parent) :
    QMainWindow {},
    m_tabWidget{new QTabWidget}
{
    setObjectName("View");
    //setUnifiedTitleAndToolBarOnMac(true);

    setDockOptions(QMainWindow::ForceTabbedDocks | QMainWindow::VerticalTabs);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    QDesktopWidget w;
    auto rect = w.availableGeometry();
    this->resize(static_cast<int>(rect.width() * 0.75),
                 static_cast<int>(rect.height() * 0.75));

    setCentralWidget(m_tabWidget);
    m_tabWidget->tabBar()->setDocumentMode(true);
    connect(m_tabWidget, &QTabWidget::currentChanged,
            [&] (int index) {
           auto view = dynamic_cast<DocumentView*>(m_tabWidget->widget(index));
           if(!view)
               return;
           emit activeDocumentChanged(view->document().model().id());
    });

    connect(m_tabWidget, &QTabWidget::tabCloseRequested, [&] (int index)
    {
        emit closeRequested(safe_cast<DocumentView*>(m_tabWidget->widget(index))->document().model().id());
    });

}

void View::setPresenter(Presenter* p)
{
    m_presenter = p;
}

void View::addDocumentView(DocumentView* doc)
{
    doc->setParent(this);
    m_tabWidget->addTab(doc, doc->document().docFileName());
    m_tabWidget->setCurrentIndex(m_tabWidget->count() - 1);
    m_tabWidget->setTabsClosable(true);
}


void View::setupPanelView(PanelView* v)
{
    using namespace std;
    QDockWidget* dial = new QDockWidget {v->defaultPanelStatus().prettyName, this};
    dial->setWidget(v->getWidget());
    dial->toggleViewAction()->setShortcut(v->shortcut());
    emit insertActionIntoMenubar({MenuInterface::name(ToplevelMenuElement::ViewMenu) + "/" +
                                  MenuInterface::name(ViewMenuElement::Windows),
                                  dial->toggleViewAction()});

    // Note : this only has meaning at initialisation time.
    auto dock = v->defaultPanelStatus().dock;

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
            { return lhs.first->defaultPanelStatus().priority < rhs.first->defaultPanelStatus().priority; });

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
            { return lhs.first->defaultPanelStatus().priority < rhs.first->defaultPanelStatus().priority; });
            tabifyDockWidget(it->second, dial);
            it->second->raise();
        }
    }

    if(!v->defaultPanelStatus().shown)
        dial->hide();
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

void iscore::View::closeEvent(QCloseEvent* ev)
{
    if(m_presenter->exit())
    {
        ev->accept();
    }
    else
    {
        ev->ignore();
    }
}

void View::on_fileNameChanged(DocumentView* d, const QString& newName)
{
    for(int i = 0; i < m_tabWidget->count(); i++)
    {
        if(d == m_tabWidget->widget(i))
        {
            QString n = newName;
            while(n.contains("/"))
            {
                n.remove(0, n.indexOf("/")+1);
            }
            n.truncate(n.lastIndexOf("."));
            m_tabWidget->setTabText(i, n);
            return;
        }
    }
}

void View::changeEvent(QEvent* ev)
{
    if(ev->type() == QEvent::ActivationChange)
    {
        emit activeWindowChanged();
    }

    QMainWindow::changeEvent(ev);
}
