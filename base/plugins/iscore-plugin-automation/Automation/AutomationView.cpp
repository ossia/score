
#include <QFlags>
#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QRect>
#include <iscore/model/Skin.hpp>
#include <qnamespace.h>

#include "AutomationView.hpp"
#include <Process/LayerView.hpp>
#include <Process/Style/ScenarioStyle.hpp>

namespace Automation
{
LayerView::LayerView(QGraphicsItem* parent) : Process::LayerView{parent}
{
  setZValue(1);
  setFlags(ItemIsSelectable | ItemIsFocusable);
  setAcceptDrops(true);

  m_textcache.setFont(ScenarioStyle::instance().Medium8Pt);
  m_textcache.setCacheEnabled(true);
}

LayerView::~LayerView()
{
}

void LayerView::setDisplayedName(const QString& s)
{
  m_textcache.setText(s);
  m_textcache.beginLayout();
  QTextLine line = m_textcache.createLine();
  line.setPosition(QPointF{0., 0.});

  m_textcache.endLayout();

  update();
}

void LayerView::paint_impl(QPainter* painter) const
{
#if !defined(ISCORE_IEEE_SKIN)
  if (m_showName)
  {
    m_textcache.draw(painter, QPointF{5., 8.});
  }
#endif
}

void LayerView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  if (event->mimeData())
    emit dropReceived(*event->mimeData());
}
}
