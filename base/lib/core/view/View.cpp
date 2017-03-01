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
#include <QQuickWidget>
#include <QWidget>
#include <algorithm>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>
#include <iscore/actions/Menu.hpp>
#include <iscore/plugins/panel/PanelDelegate.hpp>
#include <iscore/widgets/QmlContainerPanel.hpp>
#include <iscore_git_info.hpp>
#include <iterator>
#include <qcoreevent.h>
#include <qnamespace.h>
#include <set>

class QObject;

namespace iscore
{
struct PanelComparator
{
  bool operator()(
      const QPair<iscore::PanelDelegate*, QDockWidget*>& lhs,
      const QPair<iscore::PanelDelegate*, QDockWidget*>& rhs) const
  {
    return lhs.first->defaultPanelStatus().priority
           < rhs.first->defaultPanelStatus().priority;
  }
};

View::View(QObject* parent) : QMainWindow{}, m_tabWidget{new QTabWidget}
{
  setObjectName("View");
  this->setWindowIcon(QIcon("://i-score.png"));

  QString version = QString{"%1.%2.%3-%4"}
                        .arg(ISCORE_VERSION_MAJOR)
                        .arg(ISCORE_VERSION_MINOR)
                        .arg(ISCORE_VERSION_PATCH)
                        .arg(ISCORE_VERSION_EXTRA);
  auto title = tr("i-score - %1").arg(version);
  this->setWindowIconText(title);
  this->setWindowTitle(title);
  m_tabWidget->setObjectName("Documents");

  // setUnifiedTitleAndToolBarOnMac(true);

  setDockOptions(QMainWindow::ForceTabbedDocks | QMainWindow::VerticalTabs);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  QDesktopWidget w;
  auto rect = w.availableGeometry();
  this->resize(
      static_cast<int>(rect.width() * 0.75),
      static_cast<int>(rect.height() * 0.75));

  setCentralWidget(m_tabWidget);
  m_tabWidget->tabBar()->setDocumentMode(true);
  connect(
      m_tabWidget, &QTabWidget::currentChanged, this,
      [&](int index) {
        auto view = dynamic_cast<DocumentView*>(m_tabWidget->widget(index));
        if (!view)
          return;
        emit activeDocumentChanged(view->document().model().id());
      },
      Qt::QueuedConnection);

  connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, [&](int index) {
    emit closeRequested(safe_cast<DocumentView*>(m_tabWidget->widget(index))
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
  m_tabWidget->addTab(doc, doc->document().metadata().fileName());
  m_tabWidget->setCurrentIndex(m_tabWidget->count() - 1);
  m_tabWidget->setTabsClosable(true);
}

void View::setupPanel(PanelDelegate* v)
{
  using namespace std;
  auto dial = new QDockWidget{v->defaultPanelStatus().prettyName, this};
  auto w = v->widget();
  dial->setWidget(w);
  dial->toggleViewAction()->setShortcut(v->defaultPanelStatus().shortcut);

  auto& mw = v->context().menus.get().at(iscore::Menus::Windows());
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
        ISCORE_ASSERT(it != m_leftPanels.end());
        tabifyDockWidget(it->second, dial);
      }
    }
  }
  else if (dock == Qt::RightDockWidgetArea)
  {
    m_rightPanels.push_back({v, dial});

    if (m_rightPanels.size() > 1)
    {
      // Find the one with the biggest priority
      auto it = ossia::max_element(m_rightPanels, PanelComparator{});

      it->second->raise();
      if (dial != it->second)
      {
        tabifyDockWidget(dial, it->second);
      }
      else
      {
        auto it = ossia::find_if(
            m_rightPanels, [=](auto elt) { return elt.second != dial; });
        ISCORE_ASSERT(it != m_rightPanels.end());
        tabifyDockWidget(it->second, dial);
      }
    }
  }

  if (!v->defaultPanelStatus().shown)
    dial->hide();
}

void View::closeDocument(DocumentView* doc)
{
  for (int i = 0; i < m_tabWidget->count(); i++)
  {
    if (doc == m_tabWidget->widget(i))
    {
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
    if (d == m_tabWidget->widget(i))
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
  if (ev->type() == QEvent::ActivationChange)
  {
    emit activeWindowChanged();
  }

  QMainWindow::changeEvent(ev);
}

void View::resizeEvent(QResizeEvent* e)
{
  QMainWindow::resizeEvent(e);
  emit sizeChanged(e->size());
}
}
