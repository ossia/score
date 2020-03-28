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
#include <QToolTip>
#include <QApplication>
#include <QLabel>
#include <QScreen>
#include <QToolButton>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <qcoreevent.h>
#include <qnamespace.h>

#include <wobjectimpl.h>

#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolBar>
#include <set>
W_OBJECT_IMPL(score::View)
namespace score
{
struct PanelComparator
{
  bool operator()(
      const QPair<score::PanelDelegate*, QWidget*>& lhs,
      const QPair<score::PanelDelegate*, QWidget*>& rhs) const
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

class FixedTabWidget : public QWidget
{
public:
  FixedTabWidget() noexcept
  {
    this->setContentsMargins(2, 2, 2, 2);
    this->setLayout(&m_layout);
    auto layout = new QVBoxLayout;
    layout->setMargin(9);
    layout->setSpacing(6);
    layout->addWidget(&m_stack);
    m_layout.addLayout(layout);
    m_layout.addWidget(&m_buttons);
    QPalette transp = this->palette();
    transp.setColor(QPalette::Background, Qt::transparent);
    m_buttons.setPalette(transp);
    m_buttons.setContentsMargins(0, 0, 0, 0);
    m_buttons.setIconSize(QSize{24,24});
    m_actGrp = new QActionGroup{&m_buttons};
    m_actGrp->setExclusive(true);
  }

  QSize sizeHint() const override
  {
    return {200, 1000};
  }

  void setTab(int index)
  {
    m_actGrp->actions()[index]->trigger();
  }

  int addTab(QWidget* widg, const score::PanelStatus& v)
  {
    int idx = m_stack.addWidget(widg);

    auto btn = m_buttons.addAction(v.icon, v.prettyName);
    m_actGrp->addAction(btn);
    btn->setCheckable(true);
    btn->setIcon(v.icon);

    btn->setToolTip(v.prettyName);
    btn->setWhatsThis(widg->whatsThis());
    btn->setStatusTip(widg->statusTip());

    connect(btn, &QAction::triggered,
            &m_stack, [this, idx] {
      m_stack.setCurrentIndex(idx);
    });

    return idx;
  }

  void paintEvent(QPaintEvent* ev) override
  {
    QPainter p{this};
    p.setPen(Qt::transparent);
    p.setBrush(QColor("#121216"));
    p.drawRoundedRect(rect(), 3, 3);
  }
private:
  score::MarginLess<QVBoxLayout> m_layout;
  //QVBoxLayout m_layout;
  QToolBar m_buttons;
  QStackedWidget m_stack;
  QActionGroup* m_actGrp{};
};

class RectSplitter : public QSplitter
{
public:
  using QSplitter::QSplitter;
  void paintEvent(QPaintEvent* ev) override
  {
    QPainter p{this};
    p.setPen(Qt::transparent);
    p.setBrush(QColor("#121216"));
    p.drawRoundedRect(rect(), 3, 3);
  }
};
class RectWidget : public QWidget
{
public:
  void paintEvent(QPaintEvent* ev) override
  {
    QPainter p{this};
    p.setPen(Qt::transparent);
    p.setBrush(QColor("#1F1F20"));
    p.drawRect(rect());
  }
};
/*
class RectTabWidget: public QTabWidget
{
public:
  using QTabWidget::QTabWidget;
  void paintEvent(QPaintEvent* ev) override
  {
    QPainter p{this};
    p.setPen(palette().color(QPalette::Button));
    p.setBrush(QColor("#21FF25"));
    p.drawRoundedRect(rect(), 3, 3);
  }
};
*/
View::View(QObject* parent)
  : QMainWindow{}
{
  leftTabs = new FixedTabWidget;
  rightSplitter = new RectSplitter{Qt::Vertical};
  bottomTabs = new QTabWidget;
  centralTabs = new QTabWidget;
  centralTabs->setContentsMargins(0, 0, 0, 0);

  for(auto tabs : { bottomTabs, centralTabs})
  {
    tabs->setMovable(false);
    tabs->setTabsClosable(false);
  }
  setAutoFillBackground(false);
  setObjectName("View");
  this->setWindowIcon(QIcon("://ossia-score.png"));

  setTitle(*this, nullptr, false);

  auto rect = QGuiApplication::primaryScreen()->availableGeometry();
  this->resize(
      static_cast<int>(rect.width() * 0.75),
      static_cast<int>(rect.height() * 0.75));

  auto totalWidg = new RectSplitter;
  totalWidg->setContentsMargins(0,0,0,0);
  totalWidg->addWidget(leftTabs);

  {
    centralDocumentWidget = new RectWidget;
    totalWidg->addWidget(centralDocumentWidget);
    centralDocumentWidget->setContentsMargins(0, 0, 0, 0);

    auto lay = new score::MarginLess<QVBoxLayout>(centralDocumentWidget);
    lay->addWidget(centralTabs);
    centralTabs->setObjectName("Documents");
    centralTabs->setContentsMargins(0, 0, 0, 0);
    centralTabs->tabBar()->setDocumentMode(true);
    centralTabs->tabBar()->setDrawBase(false);
    centralTabs->tabBar()->setAutoHide(true);

    bottomTabs->setVisible(false);
    bottomTabs->setTabPosition(QTabWidget::South);
    bottomTabs->setTabsClosable(false);

    bottomTabs->setContentsMargins(0, 0, 0, 0);
    bottomTabs->tabBar()->setDocumentMode(false);
    bottomTabs->tabBar()->setDrawBase(false);
    bottomTabs->tabBar()->setAutoHide(true);
    bottomTabs->tabBar()->setContentsMargins(0, 0, 0, 0);

    lay->addWidget(bottomTabs);
  }
  totalWidg->addWidget(rightSplitter);
  totalWidg->setHandleWidth(1);

  setCentralWidget(totalWidg);
  connect(
      centralTabs,
      &QTabWidget::currentChanged,
      this,
      [&](int index) {
        static QMetaObject::Connection saved_connection;
        QObject::disconnect(saved_connection);
        auto widg = centralTabs->widget(index);
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

  connect(centralTabs, &QTabWidget::tabCloseRequested, this, [&](int index) {
    closeRequested(
        m_documents.at(centralTabs->widget(index))->document().model().id());
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
  centralTabs->addTab(widg, doc->document().metadata().fileName());
  centralTabs->setCurrentIndex(centralTabs->count() - 1);
  centralTabs->setTabsClosable(true);
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
    widg->setMinimumHeight(100);
    widg->setMaximumHeight(100);
    widg->setMinimumWidth(180);

    auto l = new QVBoxLayout{widg};

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
                                  "info",
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
    bottomTabs->addTab(w, v->defaultPanelStatus().icon, {});

    auto& mw = v->context().menus.get().at(score::Menus::Windows());
    auto toggle = new QAction{v->defaultPanelStatus().prettyName.toUpper(), this};
    toggle->setCheckable(true);
    toggle->setShortcut(v->defaultPanelStatus().shortcut);
    addAction(toggle);
    connect(toggle, &QAction::toggled, this, [=] (bool b) {
        bottomTabs->setVisible(b);
        bottomTabs->setCurrentWidget(w);
    });

    mw.menu()->addAction(toggle);

    if(v->defaultPanelStatus().shown)
        toggle->toggle();
    return;
  }
  const QString& tabName = v->defaultPanelStatus().prettyName.toUpper();
/*
  auto dial
      = new QDockWidget{v->defaultPanelStatus().prettyName.toUpper(), this};
  if (v->defaultPanelStatus().fixed)
    dial->setFeatures(QDockWidget::DockWidgetFeature::DockWidgetClosable);

  dial->setWidget(w);
  dial->setStatusTip(w->statusTip());
  dial->toggleViewAction()->setShortcut(v->defaultPanelStatus().shortcut);

  auto& mw = v->context().menus.get().at(score::Menus::Windows());
  mw.menu()->addAction(dial->toggleViewAction());
*/
  // Note : this only has meaning at initialisation time.
  auto dock = v->defaultPanelStatus().dock;

  switch (dock)
  {
    case Qt::LeftDockWidgetArea:
    {
      int idx = leftTabs->addTab(w, v->defaultPanelStatus());

      m_leftPanels.push_back({v, w});
      if (m_leftPanels.size() > 1)
      {
        // Find the one with the biggest priority
        auto it = ossia::max_element(m_leftPanels, PanelComparator{});

        if (w != it->second)
        {
          //leftTabs->setCurrentWidget(it->second);
        }
        else
        {
          leftTabs->setTab(idx);
        }
      }

      break;
    }
    case Qt::RightDockWidgetArea:
    {
      rightSplitter->insertWidget(0, w);
      break;
    }
    case Qt::BottomDockWidgetArea:
    {
      bottomTabs->addTab(w, v->defaultPanelStatus().icon, {});
      break;
    }
    default:
      SCORE_ABORT;
  }
  /*
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
          //tabifyDockWidget(dial, it->second);
          it->second->raise();
        }
        else
        {
          // dial is on top
          auto it = ossia::find_if(
              m_leftPanels, [=](auto elt) { return elt.second != dial; });
          SCORE_ASSERT(it != m_leftPanels.end());
          //tabifyDockWidget(it->second, dial);
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
          //addDockWidget(dock, it->second);
          it->second->raise();
        }
      }
      else
      {
        //addDockWidget(dock, dial);
      }

      break;
    }

    case Qt::TopDockWidgetArea:
    case Qt::BottomDockWidgetArea:
    default:
      SCORE_ABORT;
  }
  */
  // TODO why isn't there a title and how to access it ?

}

void View::closeDocument(DocumentView* doc)
{
  for (int i = 0; i < centralTabs->count(); i++)
  {
    auto widg = doc->viewDelegate().getWidget();
    if (widg == centralTabs->widget(i))
    {
      m_documents.erase(widg);

      centralTabs->removeTab(i);
      return;
    }
  }
}

void View::restoreLayout()
{
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
  for (int i = 0; i < centralTabs->count(); i++)
  {
    if (d->viewDelegate().getWidget() == centralTabs->widget(i))
    {
      QString n = newName;
      while (n.contains("/"))
      {
        n.remove(0, n.indexOf("/") + 1);
      }
      n.truncate(n.lastIndexOf("."));
      centralTabs->setTabText(i, n);
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
  else if (event->type() == QEvent::ToolTip)
  {
    auto tip = ((QHelpEvent*)event)->globalPos();
    auto pos = mapFromGlobal(tip);
    if(auto w = childAt(pos))
    {
      m_status->setText(w->statusTip());
    }
  }

  return QMainWindow::event(event);
}
}
