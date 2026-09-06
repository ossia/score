#include "CentralViewStack.hpp"

#include <score/widgets/MarginLess.hpp>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QPointer>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

#include <wobjectimpl.h>

#include <algorithm>

W_OBJECT_IMPL(score::CentralViewStack)

namespace score
{
CentralViewStack::CentralViewStack(
    QWidget* mainView, const QString& title, QWidget* parent)
    : QWidget{parent}
    , m_mainView{mainView}
{
  setObjectName("CentralViewStack");
  setContentsMargins(0, 0, 0, 0);

  auto lay = new score::MarginLess<QVBoxLayout>{this};

  auto navBar = new QWidget;
  navBar->setObjectName("NavigationBar");
  navBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_navLayout = new QHBoxLayout{navBar};
  m_navLayout->setContentsMargins(4, 0, 0, 0);
  m_navLayout->setSpacing(4);

  m_tabs = new QTabBar;
  m_tabs->setObjectName("CentralViewTabs");
  m_tabs->setDocumentMode(true);
  m_tabs->setDrawBase(false);
  m_tabs->setExpanding(false);
  m_tabs->setMovable(false);
  m_tabs->setUsesScrollButtons(true);
  m_tabs->setElideMode(Qt::ElideRight);
  m_tabs->setTabsClosable(true);
  m_tabs->setAutoHide(true);
  m_tabs->setFocusPolicy(Qt::NoFocus);

  m_navLayout->addStretch(1);
  m_navLayout->addWidget(m_tabs, 0, Qt::AlignRight | Qt::AlignVCenter);

  m_stack = new QStackedWidget;
  m_stack->setContentsMargins(0, 0, 0, 0);

  lay->addWidget(navBar);
  lay->addWidget(m_stack, 1);

  addView(mainView, title);
  // The document view cannot be closed from here
  m_tabs->setTabButton(0, QTabBar::RightSide, nullptr);
  m_tabs->setTabButton(0, QTabBar::LeftSide, nullptr);

  connect(m_tabs, &QTabBar::currentChanged, this, [this](int index) {
    if(index < 0 || index >= std::ssize(m_views))
      return;
    auto w = m_views[index];
    if(m_stack->currentWidget() != w)
      m_stack->setCurrentWidget(w);
    currentViewChanged(w);
  });
  connect(
      m_tabs, &QTabBar::tabCloseRequested, this,
      &CentralViewStack::on_tabCloseRequested);

  // Right-click on a tab: handled from the event directly, the tab bar
  // itself only reacts to the left button.
  m_tabs->installEventFilter(this);
}

CentralViewStack::~CentralViewStack()
{
  // The views die with the stack. Closing them first lets them tell whoever
  // opened them (a script editor resets its process's pointer from its
  // closeEvent), which plain deletion would skip; the deletion a close
  // schedules is dropped with the widget.
  const auto views = m_views;
  for(auto w : views)
    if(w != m_mainView)
      w->close();

  // Their destroyed() must not run the bookkeeping: the tab bar may already
  // be gone by then.
  for(auto w : m_views)
    disconnect(w, &QObject::destroyed, this, nullptr);
  m_views.clear();
}

bool CentralViewStack::eventFilter(QObject* obj, QEvent* ev)
{
  if(obj == m_tabs && ev->type() == QEvent::ContextMenu)
  {
    auto e = static_cast<QContextMenuEvent*>(ev);
    const int index = m_tabs->tabAt(e->pos());
    if(index > 0 && index < std::ssize(m_views))
    {
      viewContextMenuRequested(m_views[index], e->globalPos());
      e->accept();
      return true;
    }
  }
  return QWidget::eventFilter(obj, ev);
}

QWidget* CentralViewStack::mainView() const noexcept
{
  return m_mainView;
}

void CentralViewStack::setNavigationWidget(QWidget* w)
{
  if(m_navigationWidget == w)
    return;

  if(m_navigationWidget)
  {
    m_navLayout->removeWidget(m_navigationWidget);
    delete m_navigationWidget;
  }

  m_navigationWidget = w;
  if(w)
    m_navLayout->insertWidget(0, w, 0, Qt::AlignLeft | Qt::AlignVCenter);
}

QWidget* CentralViewStack::navigationWidget() const noexcept
{
  return m_navigationWidget;
}

int CentralViewStack::indexOf(QWidget* w) const noexcept
{
  auto it = std::find(m_views.begin(), m_views.end(), w);
  return it == m_views.end() ? -1 : int(std::distance(m_views.begin(), it));
}

bool CentralViewStack::hasView(QWidget* w) const noexcept
{
  return indexOf(w) != -1;
}

QWidget* CentralViewStack::currentView() const noexcept
{
  return m_stack->currentWidget();
}

QWidget* CentralViewStack::viewContaining(QWidget* w) const noexcept
{
  for(; w; w = w->parentWidget())
  {
    if(hasView(w))
      return w;
    if(w == this)
      return nullptr;
  }
  return nullptr;
}

void CentralViewStack::addView(QWidget* w, const QString& title, const QIcon& icon)
{
  if(!w || hasView(w))
    return;

  m_views.push_back(w);
  m_stack->addWidget(w);
  m_tabs->addTab(icon, title);
  m_tabs->setTabToolTip(m_tabs->count() - 1, title);

  // A view that gets deleted (WA_DeleteOnClose, process removed...) leaves
  // the stack by itself. Only the pointer value is used past this point: the
  // widget part of the object is already gone when destroyed() is emitted.
  connect(w, &QObject::destroyed, this, [this, w] { forgetView(w); });
}

void CentralViewStack::forgetView(QWidget* w)
{
  const int index = indexOf(w);
  if(index == -1)
    return;
  if(w == m_mainView)
    return;

  m_views.erase(m_views.begin() + index);

  // Not a word to the stack now: w may be a widget in the middle of its
  // destruction, still the stack's current widget until its parent hears of
  // it, and switching would have the stacked layout hide() it. The tab bar
  // picks its new current tab silently; the stack follows once the event
  // loop has let the widget go.
  {
    QSignalBlocker block{m_tabs};
    m_tabs->removeTab(index);
  }
  QTimer::singleShot(0, this, [this] { syncWithTabs(); });
}

void CentralViewStack::syncWithTabs()
{
  const int index = m_tabs->currentIndex();
  if(index < 0 || index >= std::ssize(m_views))
    return;
  auto w = m_views[index];
  if(m_stack->currentWidget() != w)
    m_stack->setCurrentWidget(w);
  currentViewChanged(w);
}

void CentralViewStack::removeView(QWidget* w)
{
  if(indexOf(w) == -1 || w == m_mainView)
    return;

  disconnect(w, &QObject::destroyed, this, nullptr);
  if(m_stack->indexOf(w) != -1)
    m_stack->removeWidget(w);
  w->hide();
  w->setParent(nullptr);

  forgetView(w);
}

void CentralViewStack::showView(QWidget* w)
{
  const int index = indexOf(w);
  if(index == -1)
    return;

  m_stack->setCurrentWidget(w);
  if(m_tabs->currentIndex() != index)
    m_tabs->setCurrentIndex(index);
  else
    currentViewChanged(w);
}

void CentralViewStack::setViewTitle(QWidget* w, const QString& title)
{
  const int index = indexOf(w);
  if(index == -1)
    return;
  m_tabs->setTabText(index, title);
  m_tabs->setTabToolTip(index, title);
}

void CentralViewStack::on_tabCloseRequested(int index)
{
  if(index <= 0 || index >= std::ssize(m_views))
    return;

  QPointer<QWidget> w = m_views[index];

  // Closing lets the view veto and clean up after itself; a view with
  // WA_DeleteOnClose gets deleted and removeView runs from destroyed().
  if(w->close() && w)
    removeView(w);
}

void CentralViewStack::releaseMainView()
{
  if(!m_mainView)
    return;

  auto w = m_mainView;
  m_mainView = nullptr;
  disconnect(w, &QObject::destroyed, this, nullptr);

  const int index = indexOf(w);
  if(index != -1)
  {
    m_views.erase(m_views.begin() + index);
    m_tabs->removeTab(index);
  }
  if(m_stack->indexOf(w) != -1)
    m_stack->removeWidget(w);
  w->hide();
  w->setParent(nullptr);
}
}
