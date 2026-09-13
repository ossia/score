#include "FixedTabWidget.hpp"

#include <score/model/Skin.hpp>
#include <score/widgets/HelpInteraction.hpp>

#include <QActionGroup>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QPainter>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <wobjectimpl.h>

#include <algorithm>

W_OBJECT_IMPL(score::FixedTabWidget)
namespace score
{

FixedTabWidget::FixedTabWidget() noexcept
    : m_buttons{new QToolBar}
{
  setContentsMargins(0, 0, 0, 0);
  m_layout.setContentsMargins(0, 0, 0, 0);
  m_layout.setSpacing(1);
  this->setLayout(&m_layout);
  auto layout = new score::MarginLess<QVBoxLayout>;

  layout->addWidget(&m_stack);
  m_layout.addLayout(layout);
  m_layout.addWidget(m_buttons);
  QPalette transp = this->palette();
  transp.setColor(QPalette::Window, Qt::transparent);
  m_buttons->setPalette(transp);
  score::setSkinIconSize(m_buttons, 24);
  m_buttons->setContentsMargins(0, 0, 0, 0);

  m_actGrp = new QActionGroup{m_buttons};
  m_actGrp->setExclusive(true);
  m_actGrp->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
}

QActionGroup* FixedTabWidget::actionGroup() const noexcept
{
  return m_actGrp;
}

QToolBar* FixedTabWidget::toolbar() const noexcept
{
  return m_buttons;
}

QSize FixedTabWidget::sizeHint() const
{
  return {200, 1000};
}

void FixedTabWidget::setTab(int index)
{
  if(m_actGrp->actions()[index]->isChecked())
    return;

  m_actGrp->actions()[index]->trigger();
}

struct DragOverToolButton final : public QToolButton
{
  explicit DragOverToolButton(QWidget* parent = nullptr) noexcept
      : QToolButton{parent}
  {
    setAcceptDrops(true);
  }

  void dragEnterEvent(QDragEnterEvent* e) override
  {
    m_tm = startTimer(250);
    QToolButton::dragEnterEvent(e);
    e->accept();
  }

  void dragLeaveEvent(QDragLeaveEvent* e) override
  {
    if(m_tm != 0)
      killTimer(m_tm);
    m_tm = 0;

    QToolButton::dragLeaveEvent(e);
    e->accept();
  }

  void timerEvent(QTimerEvent* e) override
  {
    if(m_tm != 0)
      killTimer(m_tm);
    m_tm = 0;

    click();
  }

  int m_tm = 0;
};

std::pair<int, QAction*>
FixedTabWidget::addTab(QWidget* widg, const PanelStatus& v, int index)
{
  int idx = index >= 0 ? m_stack.insertWidget(index, widg) : m_stack.addWidget(widg);

  auto bbtn = new DragOverToolButton{};
  bbtn->setAutoRaise(true);
  bbtn->setFocusPolicy(Qt::NoFocus);
  bbtn->setIconSize(m_buttons->iconSize());
  bbtn->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(
      bbtn, &QWidget::customContextMenuRequested, this, [this, bbtn, widg](QPoint p) {
    tabContextMenuRequested(widg, bbtn->mapToGlobal(p));
  });

  QAction* btn{};
  if(index < 0)
  {
    btn = m_buttons->addWidget(bbtn);
  }
  else
  {
    const auto& acts = m_buttons->actions();
    int k = 0;
    QAction* before{};
    for(QAction* act : acts)
    {
      if(k++ == index)
      {
        before = act;
        break;
      }
    }
    if(before)
    {
      btn = m_buttons->insertWidget(before, bbtn);
    }
    else
    {
      btn = m_buttons->addWidget(bbtn);
    }
  }
  bbtn->setDefaultAction(btn);
  btn->setIcon(v.icon);
  btn->setText(v.prettyName);
  btn->setIconText(v.prettyName);

  m_actGrp->addAction(btn);
  btn->setCheckable(true);
  btn->setShortcut(v.shortcut);
  btn->setIcon(v.icon);

  btn->setToolTip(v.prettyName);
  btn->setWhatsThis(widg->whatsThis());
  btn->setStatusTip(widg->statusTip());

  connect(btn, &QAction::triggered, &m_stack, [this, btn, widg](bool checked) {
    m_stack.setCurrentWidget(widg);
    actionTriggered(btn, checked);
  });

  m_widgetActions.emplace_back(widg, btn);
  // A tab widget that gets deleted takes its tab with it. Only the pointer
  // value is used then: the widget part is already gone in destroyed().
  connect(widg, &QObject::destroyed, this, [this, widg] { forgetTab(widg); });
  return std::make_pair(idx, btn);
}

QAction* FixedTabWidget::actionFor(QWidget* widg) const noexcept
{
  for(auto& [w, act] : m_widgetActions)
    if(w == widg)
      return act;
  return nullptr;
}

void FixedTabWidget::showTab(QWidget* widg)
{
  auto act = actionFor(widg);
  if(!act)
    return;

  if(act->isChecked())
  {
    // QAction::trigger() would uncheck it (the group is ExclusiveOptional)
    m_stack.setCurrentWidget(widg);
    actionTriggered(act, true);
  }
  else
  {
    act->trigger();
  }
}

void FixedTabWidget::removeTab(QWidget* widg)
{
  if(!actionFor(widg))
    return;

  disconnect(widg, &QObject::destroyed, this, nullptr);
  if(m_stack.indexOf(widg) != -1)
    m_stack.removeWidget(widg);
  forgetTab(widg);
}

void FixedTabWidget::forgetTab(QWidget* widg)
{
  auto act = actionFor(widg);
  if(!act)
    return;

  std::erase_if(m_widgetActions, [=](const auto& p) { return p.first == widg; });

  const bool wasCurrent = act->isChecked();
  m_actGrp->removeAction(act);
  m_buttons->removeAction(act);
  // The action owns the tab's button, which may be the one dispatching a
  // context menu right now (moving a tab out from its own menu)
  act->deleteLater();

  // Fall back on the first tab, in the order of the buttons. Deferred: the
  // widget may be in the middle of its destruction and still the stack's
  // current widget, which the switch would have the stacked layout hide().
  if(wasCurrent)
  {
    QTimer::singleShot(0, this, [this] {
      // Another tab was shown in the meantime
      if(m_actGrp->checkedAction())
        return;
      for(auto first : m_buttons->actions())
      {
        for(auto& [w, a] : m_widgetActions)
        {
          if(a == first)
          {
            showTab(w);
            return;
          }
        }
      }
    });
  }
}

QWidget* FixedTabWidget::currentWidget() const noexcept
{
  return m_stack.currentWidget();
}

QAction* FixedTabWidget::addAction(QWidget* widg, const PanelStatus& v)
{
  auto btn = m_buttons->addAction(v.icon, v.prettyName);
  m_actGrp->addAction(btn);
  btn->setCheckable(true);
  btn->setShortcut(v.shortcut);
  btn->setIcon(v.icon);

  score::setHelp(btn, v.prettyName);
  btn->setWhatsThis(widg->whatsThis());
  btn->setStatusTip(widg->statusTip());

  return btn;
}

QAction* FixedTabWidget::addAction(QAction* act)
{
  m_buttons->addAction(act);
  return act;
}

void FixedTabWidget::paintEvent(QPaintEvent* ev)
{
  if(brush == QBrush())
    return;
  QPainter p{this};
  p.setPen(Qt::transparent);
  // p.setBrush(QColor("#121216"));
  p.setBrush(brush);
  p.drawRoundedRect(rect(), 3, 3);
}

}
