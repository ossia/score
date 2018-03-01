// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TextItem.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QTextLayout>
#include <QPainter>
#include <Process/Style/ScenarioStyle.hpp>
namespace Scenario
{

TextItem::TextItem(QString text, QGraphicsItem* parent)
    : QGraphicsTextItem{text, parent}
{
  this->setFlag(QGraphicsItem::ItemIsFocusable);
  this->setDefaultTextColor(Qt::white);
}

void TextItem::focusOutEvent(QFocusEvent* event)
{
  focusOut();
}

SimpleTextItem::SimpleTextItem(QGraphicsItem* p): QGraphicsItem{p}
{
  m_color = ScenarioStyle::instance().ConditionWaiting;
}

QRectF SimpleTextItem::boundingRect() const
{
  return m_rect;
}

void SimpleTextItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if(m_line)
  {
    auto& skin = ScenarioStyle::instance();
    skin.TextItemPen.setBrush(m_color.getBrush());
    painter->setPen(skin.TextItemPen);
    painter->setBrush(skin.NoBrush);
    painter->drawGlyphRun({0, 0}, *m_line);
  }
}

void SimpleTextItem::setFont(QFont f)
{
  m_font = std::move(f);
  m_font.setStyleStrategy(QFont::PreferAntialias);
  updateImpl();
}

void SimpleTextItem::setText(QString s)
{
  m_string = std::move(s);
  updateImpl();
}

void SimpleTextItem::setColor(score::ColorRef c)
{
  m_color = c;
  update();
}

void SimpleTextItem::updateImpl()
{
  prepareGeometryChange();

  if(m_string.isEmpty())
  {
    m_rect = QRectF{};
    m_line = ossia::none;
  }
  else
  {
    QTextLayout layout(m_string, m_font);
    layout.beginLayout();
    auto line = layout.createLine();
    layout.endLayout();

    m_rect = line.naturalTextRect();
    auto r = line.glyphRuns();
    if(r.size() > 0)
      m_line = std::move(r[0]);
    else
      m_line = ossia::none;
  }

  update();
}
}
