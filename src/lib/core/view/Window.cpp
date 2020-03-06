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

#include <QCloseEvent>
#include <QDockWidget>
#include <QEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <qcoreevent.h>
#include <qnamespace.h>

#include <wobjectimpl.h>

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
  setAutoFillBackground(false);
  //setAttribute(Qt::WA_OpaquePaintEvent);
  setObjectName("View");
  this->setWindowIcon(QIcon("://ossia-score.png"));
  m_tabWidget->setObjectName("Documents");

  setTitle(*this, nullptr, false);

  // setUnifiedTitleAndToolBarOnMac(true);

  setDockOptions(QMainWindow::VerticalTabs | QMainWindow::ForceTabbedDocks);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  auto rect = QGuiApplication::primaryScreen()->availableGeometry();
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

  m_bottomTabs = new QTabWidget;
  m_bottomTabs->setVisible(false);
  m_bottomTabs->setTabPosition(QTabWidget::South);
  m_bottomTabs->setTabsClosable(false);

  m_bottomTabs->setContentsMargins(0, 0, 0, 0);
  m_bottomTabs->tabBar()->setDocumentMode(false);
  m_bottomTabs->tabBar()->setDrawBase(false);
  m_bottomTabs->tabBar()->setAutoHide(true);
  m_bottomTabs->tabBar()->setContentsMargins(0, 0, 0, 0);

  lay->addWidget(m_bottomTabs);
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

class HelperPanelDelegate : public PanelDelegate
{
public:
  HelperPanelDelegate(const score::GUIApplicationContext& ctx)
      : PanelDelegate{ctx}
  {
    widg = new QWidget;
    widg->setContentsMargins(3, 2, 3, 2);
    widg->setMinimumHeight(60);
    widg->setMaximumHeight(60);
    widg->setMinimumWidth(250);

    auto l = new score::MarginLess<QVBoxLayout>{widg};

    status = new QLabel;
    status->setTextFormat(Qt::RichText);
    status->setText("<i>Remember those quiet evenings</i>");
    status->setWordWrap(true);
    status->setStyleSheet("color: #787876;");
    l->addWidget(status);
    l->addStretch(12);
  }

  QWidget* widget() override { return widg; }

  const PanelStatus& defaultPanelStatus() const override
  {
    static const PanelStatus stat{true,
                                  true,
                                  Qt::RightDockWidgetArea,
                                  -100000,
                                  "Info",
                                  QKeySequence::HelpContents};
    return stat;
  }
  QWidget* widg{};
  QLabel* status{};
};
void View::setupPanel(PanelDelegate* v)
{
  {
    // First time we get there, register the additional helper panel
    static int ok = false;
    if (!ok)
    {
      ok = true;
      static HelperPanelDelegate hd(v->context());
      m_status = hd.status;
      setupPanel(&hd);
    }
  }
  using namespace std;
  auto w = v->widget();

  if (v->defaultPanelStatus().dock == Qt::BottomDockWidgetArea)
  {
    m_bottomTabs->addTab(w, v->defaultPanelStatus().prettyName);

    auto& mw = v->context().menus.get().at(score::Menus::Windows());
    auto toggle = new QAction{v->defaultPanelStatus().prettyName.toUpper(), this};
    toggle->setCheckable(true);
    toggle->setShortcut(v->defaultPanelStatus().shortcut);
    addAction(toggle);
    connect(toggle, &QAction::toggled, this, [=] (bool b) {
        m_bottomTabs->setVisible(b);
        m_bottomTabs->setCurrentWidget(w);
    });

    mw.menu()->addAction(toggle);

    if(v->defaultPanelStatus().shown)
        toggle->toggle();
    return;
  }

  auto dial
      = new QDockWidget{v->defaultPanelStatus().prettyName.toUpper(), this};
  if (v->defaultPanelStatus().fixed)
    dial->setFeatures(QDockWidget::DockWidgetFeature::DockWidgetClosable);

  dial->setWidget(w);
  dial->setStatusTip(w->statusTip());
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

        if (dial != it->second)
        {
          // dial is not on top
          tabifyDockWidget(dial, it->second);
          it->second->raise();
        }
        else
        {
          // dial is on top
          auto it = ossia::find_if(
              m_leftPanels, [=](auto elt) { return elt.second != dial; });
          SCORE_ASSERT(it != m_leftPanels.end());
          tabifyDockWidget(it->second, dial);
          dial->raise();
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
  // TODO why isn't there a title and how to access it ?

  if (auto title = dial->titleBarWidget())
    title->setStatusTip(w->statusTip());

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
bool score::View::event(QEvent* event)
{
  if (event->type() == QEvent::StatusTip)
  {
    auto tip = ((QStatusTipEvent*)event)->tip();
    auto idx = tip.indexOf(QChar('\n'));
    if (idx != -1)
    {
      tip.insert(idx, "</b>\n");
      tip.push_front("<b>");
    }
    else
    {
      tip.push_front("<b>");
      tip.push_back("</b>");
    }
    tip.replace(QChar('\n'), "</br>");
    m_status->setText(tip);
  }

  return QMainWindow::event(event);
}
}
