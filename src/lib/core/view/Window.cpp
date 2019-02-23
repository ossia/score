// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/actions/Menu.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/widgets/MarginLess.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/Window.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QDockWidget>
#include <QEvent>
#include <QFlags>
#include <QRect>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <qcoreevent.h>
#include <qnamespace.h>

#include <wobjectimpl.h>

#include <algorithm>
#include <iterator>
#include <set>
W_OBJECT_IMPL(score::View)
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
View::~View() {}

static void
setTitle(View& view, const score::Document* document, bool save_state) noexcept
{
  QString title;

  if (document)
  {
    if (save_state)
    {
      title = " * ";
    }
    title += QStringLiteral("score %1 - %2")
                 .arg(qApp->applicationVersion())
                 .arg(document->metadata().fileName());
  }
  else
  {
    title = QStringLiteral("score %1").arg(qApp->applicationVersion());
  }

  view.setWindowIconText(title);
  view.setWindowTitle(title);
}

View::View(QObject* parent) : QMainWindow{}, m_tabWidget{new QTabWidget}
{
  setObjectName("View");
  this->setWindowIcon(QIcon("://ossia-score.png"));
  m_tabWidget->setObjectName("Documents");

  setTitle(*this, nullptr, false);

  // setUnifiedTitleAndToolBarOnMac(true);

  setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::VerticalTabs);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  QDesktopWidget w;
  auto rect = w.availableGeometry();
  this->resize(
      static_cast<int>(rect.width() * 0.75),
      static_cast<int>(rect.height() * 0.75));

  auto centralWidg = new QWidget;
  centralWidg->setContentsMargins(0, 0, 0, 0);
  auto lay = new score::MarginLess<QVBoxLayout>(centralWidg);
  lay->addWidget(m_tabWidget);
  setCentralWidget(centralWidg);
  m_tabWidget->setContentsMargins(0, 0, 0, 0);
  m_tabWidget->tabBar()->setDocumentMode(true);
  m_tabWidget->tabBar()->setDrawBase(false);
  m_tabWidget->tabBar()->setAutoHide(true);
  connect(
      m_tabWidget,
      &QTabWidget::currentChanged,
      this,
      [&](int index) {
        static QMetaObject::Connection saved_connection;
        QObject::disconnect(saved_connection);
        auto widg = m_tabWidget->widget(index);
        auto doc = m_documents.find(widg);
        if (doc == m_documents.end())
        {
          setTitle(*this, nullptr, false);
          return;
        }

        auto& document = const_cast<score::Document&>(doc->second->document());
        activeDocumentChanged(document.model().id());

        setTitle(*this, &document, !document.commandStack().isAtSavedIndex());
        saved_connection = connect(
            &document.commandStack(),
            &score::CommandStack::saveIndexChanged,
            this,
            [this, doc = &document](bool state) {
              setTitle(*this, doc, !state);
            });
      },
      Qt::QueuedConnection);

  connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, [&](int index) {
    closeRequested(
        m_documents.at(m_tabWidget->widget(index))->document().model().id());
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
  auto dial
      = new QDockWidget{v->defaultPanelStatus().prettyName.toUpper(), this};
  auto w = v->widget();
  dial->setWidget(w);
  dial->toggleViewAction()->setShortcut(v->defaultPanelStatus().shortcut);

  auto& mw = v->context().menus.get().at(score::Menus::Windows());
  mw.menu()->addAction(dial->toggleViewAction());

  // Note : this only has meaning at initialisation time.
  auto dock = v->defaultPanelStatus().dock;

  switch (dock)
  {
    case Qt::LeftDockWidgetArea:
    {
      addDockWidget(dock, dial);
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
      break;
    }

    case Qt::RightDockWidgetArea:
    {
      m_rightPanels.push_back({v, dial});
      if (m_rightPanels.size() > 1)
      {
        ossia::sort(m_rightPanels, PanelComparator{});
        for (auto it = m_rightPanels.rbegin(); it != m_rightPanels.rend();
             ++it)
        {
          addDockWidget(dock, it->second);
          it->second->raise();
        }
      }
      else
      {
        addDockWidget(dock, dial);
      }

      break;
    }

    case Qt::TopDockWidgetArea:
    {
      addDockWidget(dock, dial);
      break;
    }

    case Qt::BottomDockWidgetArea:
    {
      addDockWidget(dock, dial);
      break;
    }

    default:
    {
      addDockWidget(dock, dial);
      break;
    }
  }

  if (!v->defaultPanelStatus().shown)
    dial->hide();
}

void View::closeDocument(DocumentView* doc)
{
  for (int i = 0; i < m_tabWidget->count(); i++)
  {
    auto widg = doc->viewDelegate().getWidget();
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
  if (m_presenter)
    if (ev->type() == QEvent::ActivationChange)
    {
      for (GUIApplicationPlugin* ctrl :
           m_presenter->applicationContext().guiApplicationPlugins())
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
