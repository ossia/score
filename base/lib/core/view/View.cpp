// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/detail/algorithms.hpp>
#include <QAction>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QDockWidget>
#include <QEvent>
#include <QFlags>
#include <QRect>
#include <QTabBar>
#include <QTabWidget>
#include <QWidget>
#include <algorithm>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>

#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/actions/Menu.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score_git_info.hpp>
#include <iterator>
#include <qcoreevent.h>
#include <qnamespace.h>
#include <set>
#include <score/plugins/application/GUIApplicationPlugin.hpp>

namespace score
{
struct PanelComparator
{
  bool operator()(
      const QPair<score::PanelDelegate*, QDockWidget*>& lhs,
      const QPair<score::PanelDelegate*, QDockWidget*>& rhs) const
  {
    return lhs.first->defaultPanelStatus().priority
           < rhs.first->defaultPanelStatus().priority;
  }
};
View::~View()
{

}
View::View(QObject* parent) : QMainWindow{}, m_tabWidget{new QTabWidget}
{
  setObjectName("View");
  this->setWindowIcon(QIcon("://score.png"));

  QString version = QString{"%1.%2.%3-%4"}
                        .arg(SCORE_VERSION_MAJOR)
                        .arg(SCORE_VERSION_MINOR)
                        .arg(SCORE_VERSION_PATCH)
                        .arg(SCORE_VERSION_EXTRA);
  auto title = tr("score - %1").arg(version);
  this->setWindowIconText(title);
  this->setWindowTitle(title);
  m_tabWidget->setObjectName("Documents");

  // setUnifiedTitleAndToolBarOnMac(true);

  setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::VerticalTabs);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  QDesktopWidget w;
  auto rect = w.availableGeometry();
  this->resize(
      static_cast<int>(rect.width() * 0.75),
      static_cast<int>(rect.height() * 0.75));

  setCentralWidget(m_tabWidget);
  m_tabWidget->setContentsMargins(0, 0, 0, 0);
  m_tabWidget->tabBar()->setDocumentMode(true);
  m_tabWidget->tabBar()->setDrawBase(false);
  m_tabWidget->tabBar()->setAutoHide(true);
  connect(
      m_tabWidget, &QTabWidget::currentChanged, this,
      [&](int index) {
        auto widg = m_tabWidget->widget(index);
        auto doc = m_documents.find(widg);
        if(doc == m_documents.end())
          return;

        activeDocumentChanged(doc->second->document().model().id());
      },
      Qt::QueuedConnection);

  connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, [&](int index) {
    closeRequested(m_documents.at(m_tabWidget->widget(index))
                            ->document()
                            .model()
                            .id());
  });
}

void View::setPresenter(Presenter* p)
{
  m_presenter = p;
}
void View::addDocumentView(DocumentView* doc)
{
  doc->setParent(this);
  auto widg = doc->viewDelegate().getWidget();
  m_documents.insert(std::make_pair(widg, doc));
  m_tabWidget->addTab(widg, doc->document().metadata().fileName());
  m_tabWidget->setCurrentIndex(m_tabWidget->count() - 1);
  m_tabWidget->setTabsClosable(true);
  sizeChanged(size());
}

void View::setupPanel(PanelDelegate* v)
{
  using namespace std;
  auto dial = new QDockWidget{v->defaultPanelStatus().prettyName, this};
  auto w = v->widget();
  dial->setWidget(w);
  dial->toggleViewAction()->setShortcut(v->defaultPanelStatus().shortcut);

  auto& mw = v->context().menus.get().at(score::Menus::Windows());
  mw.menu()->addAction(dial->toggleViewAction());

  // Note : this only has meaning at initialisation time.
  auto dock = v->defaultPanelStatus().dock;

  this->addDockWidget(dock, dial);
  if (dock == Qt::LeftDockWidgetArea)
  {
    m_leftPanels.push_back({v, dial});
    if (m_leftPanels.size() > 1)
    {
      // Find the one with the biggest priority
      auto it = ossia::max_element(m_leftPanels, PanelComparator{});

      it->second->raise();
      if (dial != it->second)
      {
        // dial is not on top
        tabifyDockWidget(dial, it->second);
      }
      else
      {
        // dial is on top
        auto it = ossia::find_if(
            m_leftPanels, [=](auto elt) { return elt.second != dial; });
        SCORE_ASSERT(it != m_leftPanels.end());
        tabifyDockWidget(it->second, dial);
      }
    }
  }
  else if (dock == Qt::RightDockWidgetArea)
  {
    m_rightPanels.push_back({v, dial});
  }

  if (!v->defaultPanelStatus().shown)
    dial->hide();
}

void View::closeDocument(DocumentView* doc)
{
  for (int i = 0; i < m_tabWidget->count(); i++)
  {
    auto widg = doc->viewDelegate().getWidget() ;
    if (widg == m_tabWidget->widget(i))
    {
      m_documents.erase(widg);

      m_tabWidget->removeTab(i);
      return;
    }
  }
}

void View::restoreLayout()
{
  for (auto panels : {m_leftPanels, m_rightPanels})
  {
    ossia::sort(panels, PanelComparator{});

    for (auto& panel : panels)
    {
      auto dock = panel.second;
      if (dock->isFloating())
      {
        dock->setFloating(false);
      }
    }
  }
}

void View::closeEvent(QCloseEvent* ev)
{
  if (m_presenter->exit())
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
  for (int i = 0; i < m_tabWidget->count(); i++)
  {
    if (d->viewDelegate().getWidget() == m_tabWidget->widget(i))
    {
      QString n = newName;
      while (n.contains("/"))
      {
        n.remove(0, n.indexOf("/") + 1);
      }
      n.truncate(n.lastIndexOf("."));
      m_tabWidget->setTabText(i, n);
      return;
    }
  }
}

void View::changeEvent(QEvent* ev)
{
  if(m_presenter)
  if (ev->type() == QEvent::ActivationChange)
  {
    for(GUIApplicationPlugin* ctrl : m_presenter->applicationContext().guiApplicationPlugins())
    {
      ctrl->on_activeWindowChanged();
    }
  }

  QMainWindow::changeEvent(ev);
}

void View::resizeEvent(QResizeEvent* e)
{
  QMainWindow::resizeEvent(e);
  sizeChanged(e->size());
}

}
