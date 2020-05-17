#include "FixedTabWidget.hpp"

#include <QPainter>
#include <QToolBar>
#include <QActionGroup>
#include <wobjectimpl.h>

W_OBJECT_IMPL(score::FixedTabWidget)
namespace score
{

FixedTabWidget::FixedTabWidget() noexcept
  : m_buttons{new QToolBar}
{
  m_layout.setMargin(10);
  m_layout.setSpacing(6);
  this->setLayout(&m_layout);
  auto layout = new score::MarginLess<QVBoxLayout>;

  layout->addWidget(&m_stack);
  m_layout.addLayout(layout);
  m_layout.addWidget(m_buttons);
  QPalette transp = this->palette();
  transp.setColor(QPalette::Background, Qt::transparent);
  m_buttons->setPalette(transp);
  m_buttons->setIconSize(QSize{24,24});
  m_buttons->setContentsMargins(0, 0, 0, 0);

  m_actGrp = new QActionGroup{m_buttons};
  m_actGrp->setExclusive(true);
#if QT_VERSION > QT_VERSION_CHECK(5, 14, 0)
  m_actGrp->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
#endif
}

QActionGroup* FixedTabWidget::actionGroup() const noexcept { return m_actGrp; }

QToolBar* FixedTabWidget::toolbar() const noexcept { return m_buttons; }

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


std::pair<int, QAction*> FixedTabWidget::addTab(QWidget* widg, const PanelStatus& v)
{
  int idx = m_stack.addWidget(widg);

  auto btn = m_buttons->addAction(v.icon, v.prettyName);
  m_actGrp->addAction(btn);
  btn->setCheckable(true);
  btn->setShortcut(v.shortcut);
  btn->setIcon(v.icon);

  btn->setToolTip(v.prettyName);
  btn->setWhatsThis(widg->whatsThis());
  btn->setStatusTip(widg->statusTip());

  connect(btn, &QAction::triggered,
          &m_stack, [this, btn, idx] (bool checked) {
    m_stack.setCurrentIndex(idx);
    actionTriggered(btn, checked);
  });

  return std::make_pair(idx, btn);
}

QAction* FixedTabWidget::addAction(QWidget* widg, const PanelStatus& v)
{
  auto btn = m_buttons->addAction(v.icon, v.prettyName);
  m_actGrp->addAction(btn);
  btn->setCheckable(true);
  btn->setShortcut(v.shortcut);
  btn->setIcon(v.icon);

  btn->setToolTip(v.prettyName);
  btn->setWhatsThis(widg->whatsThis());
  btn->setStatusTip(widg->statusTip());

  return btn;
}


void FixedTabWidget::paintEvent(QPaintEvent* ev)
{
  if(brush == QBrush()) return;
  QPainter p{this};
  p.setPen(Qt::transparent);
  //p.setBrush(QColor("#121216"));
  p.setBrush(brush);
  p.drawRoundedRect(rect(), 3, 3);
}

}
