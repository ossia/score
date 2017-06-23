
#include <QFlags>
#include <QFont>
#include <QGraphicsItem>
#include <QPainter>
#include <QRect>
#include <qnamespace.h>

#include "MappingView.hpp"
#include <Process/LayerView.hpp>
#include <Process/Style/ScenarioStyle.hpp>

namespace Mapping
{
LayerView::LayerView(QGraphicsItem* parent) : Process::LayerView{parent}
{
  setZValue(1);
  this->setFlags(ItemClipsToShape |
      ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);

  m_textcache.setFont(ScenarioStyle::instance().Medium8Pt);
  m_textcache.setCacheEnabled(true);
}

void LayerView::showName(bool b)
{
  m_showName = b;

  update();
}

void LayerView::setDisplayedName(const QString& s)
{
  m_displayedName = s;

  // TODO refactor with automation
  m_textcache.setText(s);
  m_textcache.beginLayout();
  QTextLine line = m_textcache.createLine();
  line.setPosition(QPointF{0., 0.});

  m_textcache.endLayout();

  update();
}

void LayerView::paint_impl(QPainter* painter) const
{
  if (m_showName)
  {
    painter->setPen(ScenarioStyle::instance().ConstraintHeaderSeparator);
    m_textcache.draw(painter, QPointF{5., 8.});
  }
}
}
