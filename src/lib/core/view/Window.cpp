// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "HelperPanelDelegate.hpp"

#include <score/actions/Menu.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/widgets/MarginLess.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/FixedTabWidget.hpp>
#include <core/view/Window.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyleOptionTitleBar>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>
#include <qcoreevent.h>
#include <qnamespace.h>

#include <wobjectimpl.h>

#include <set>
W_OBJECT_IMPL(score::View)
namespace score
{
View::~View() { }

static void setTitle(View& view, const score::Document* document, bool save_state) noexcept
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

class RectSplitter : public QSplitter
{
public:
  QBrush brush;
  using QSplitter::QSplitter;
  void paintEvent(QPaintEvent* ev) override
  {
    if (brush == QBrush())
      return;
    QPainter p{this};
    p.setPen(Qt::transparent);
    p.setBrush(brush);
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
class BottomToolbarWidget : public QWidget
{
public:
  void paintEvent(QPaintEvent* ev) override
  {
    QPainter p{this};
    p.fillRect(rect(), Qt::transparent);
  }
};

class TitleBar : public QWidget
{
public:
  TitleBar() { setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed); }

  void setText(const QString& txt)
  {
    m_text = txt.toUpper();
    update();
  }

  QSize sizeHint() const override { return {100, 19}; }

  void paintEvent(QPaintEvent* ev) override
  {
    static const QFont font("Ubuntu", 11, QFont::Bold);
    QPainter painter{this};
    painter.setFont(font);
    painter.drawText(rect(), Qt::AlignCenter, m_text);
  }

private:
  QString m_text;
};

View::View(QObject* parent) : QMainWindow{}
{
  setAutoFillBackground(false);
  setObjectName("View");
  setWindowIcon(QIcon("://ossia-score.png"));
  setTitle(*this, nullptr, false);

  setIconSize(QSize{24, 24});

  topleftToolbar = new QWidget;
  topleftToolbar->setLayout(new score::MarginLess<QHBoxLayout>);
  auto leftLabel = new TitleBar;
  leftTabs = new FixedTabWidget;
  connect(leftTabs, &FixedTabWidget::actionTriggered, this, [=](QAction* act, bool b) {
    leftLabel->setText(act->text());
  });
  ((QVBoxLayout*)leftTabs->layout())->insertWidget(0, topleftToolbar);
  ((QVBoxLayout*)leftTabs->layout())->insertWidget(1, leftLabel);
  rightSplitter = new RectSplitter{Qt::Vertical};
  auto rect = QGuiApplication::primaryScreen()->availableGeometry();
  this->resize(static_cast<int>(rect.width() * 0.75), static_cast<int>(rect.height() * 0.75));

  auto totalWidg = new RectSplitter;
  totalWidg->setContentsMargins(0, 0, 0, 0);
  totalWidg->addWidget(leftTabs);

  {
    auto rs = new RectSplitter{Qt::Vertical};
    rs->brush = QColor("#1D1D1D");
    centralDocumentWidget = rs;
    totalWidg->addWidget(centralDocumentWidget);
    centralDocumentWidget->setContentsMargins(0, 0, 0, 0);

    // auto lay = new score::MarginLess<QVBoxLayout>(centralDocumentWidget);
    centralTabs = new QTabWidget;
    centralTabs->setContentsMargins(0, 0, 0, 0);
    centralTabs->setMovable(false);

    centralTabs->setObjectName("Documents");
    centralTabs->setContentsMargins(0, 0, 0, 0);
    centralTabs->tabBar()->setDocumentMode(true);
    centralTabs->tabBar()->setDrawBase(false);
    centralTabs->tabBar()->setAutoHide(true);
    // centralTabs->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    rs->addWidget(centralTabs);

    transportBar = new BottomToolbarWidget;
    auto transportLay = new score::MarginLess<QGridLayout>{transportBar};
    QPalette pal;
    pal.setColor(QPalette::Window, Qt::blue);
    transportBar->setPalette(pal);
    transportBar->setFixedHeight(35);
    rs->addWidget(transportBar);

    bottomTabs = new FixedTabWidget;
    bottomTabs->brush = qApp->palette().brush(QPalette::Window);
    bottomTabs->setContentsMargins(0, 0, 0, 0);
    bottomTabs->setMaximumHeight(300);
#if QT_VESION >= QT_VERSION_CHECK(5, 14, 0)
    bottomTabs->actionGroup()->setExclusionPolicy(
        QActionGroup::ExclusionPolicy::ExclusiveOptional);
#endif
    rs->addWidget(bottomTabs);
    rs->setCollapsible(0, false);
    rs->setCollapsible(1, false);
    rs->setCollapsible(2, true);
    QList<int> sz = rs->sizes();
    sz[2] = 0;
    rs->setSizes(sz);
    connect(
        bottomTabs,
        &FixedTabWidget::actionTriggered,
        this,
        [rs, leftLabel, this](QAction* act, bool ok) {
          if (ok)
          {
            QList<int> sz = rs->sizes();
            if (sz[2] <= 1)
            {
              sz[2] = 200;
              rs->setSizes(sz);
            }
          }
          else
          {
            QList<int> sz = rs->sizes();
            sz[2] = 0;
            rs->setSizes(sz);
          }
        });
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
            [this, doc = &document](bool state) { setTitle(*this, doc, !state); });
      },
      Qt::QueuedConnection);

  connect(centralTabs, &QTabWidget::tabCloseRequested, this, [&](int index) {
    closeRequested(m_documents.at(centralTabs->widget(index))->document().model().id());
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

  QAction* toggle{};
  // Add the panel
  switch (v->defaultPanelStatus().dock)
  {
    case Qt::LeftDockWidgetArea:
    {
      auto [idx, act] = leftTabs->addTab(w, v->defaultPanelStatus());
      toggle = act;

      break;
    }
    case Qt::RightDockWidgetArea:
    {
      rightSplitter->insertWidget(0, w);

      break;
    }
    case Qt::BottomDockWidgetArea:
    {
      auto [tabIdx, act] = bottomTabs->addTab(w, v->defaultPanelStatus());
      toggle = act;

      break;
    }
    default:
      SCORE_ABORT;
  }

  if (toggle)
  {
    auto& mw = v->context().menus.get().at(score::Menus::Windows());
    addAction(toggle);
    mw.menu()->addAction(toggle);

    // Maybe show the panel
    if (v->defaultPanelStatus().shown)
      toggle->toggle();
  }
}

void View::allPanelsAdded()
{
  for (auto& panel : score::GUIAppContext().panels())
  {
    if (panel.defaultPanelStatus().prettyName == QObject::tr("Inspector"))
    {
      auto splitter = (RectSplitter*)centralWidget();
      rightSplitter->insertWidget(1, panel.widget());

      auto act = bottomTabs->addAction(rightSplitter->widget(1), panel.defaultPanelStatus());
      bottomTabs->actionGroup()->removeAction(act);
      act->setChecked(true);
      connect(act, &QAction::toggled, this, [splitter](bool ok) {
        QList<int> sz = splitter->sizes();
        if (ok)
        {
          if (sz[2] <= 1)
          {
            sz[2] = 200;
            splitter->setSizes(sz);
          }
        }
        else
        {
          sz[2] = 0;
          splitter->setSizes(sz);
        }
      });
      break;
    }
  }

  // Show the device explorer first
  leftTabs->toolbar()->actions().front()->trigger();
}

void View::addTopToolbar(QToolBar* b)
{
  b->setFloatable(false);
  b->setMovable(false);
  topleftToolbar->layout()->addWidget(b);

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

void View::restoreLayout() { }

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
      for (GUIApplicationPlugin* ctrl : m_presenter->applicationContext().guiApplicationPlugins())
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
    if (auto w = childAt(pos))
    {
      m_status->setText(w->statusTip());
    }
  }

  return QMainWindow::event(event);
}

}
