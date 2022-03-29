// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TextItem.hpp"

#include <score/graphics/GraphicsItem.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QTextLayout>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::TextItem)
W_OBJECT_IMPL(score::QGraphicsTextButton)
namespace score
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

SimpleTextItem::SimpleTextItem(const score::BrushSet& col, QGraphicsItem* p)
    : QGraphicsItem{p}
    , m_color{&col}
{
  setFont(score::Skin::instance().Medium8Pt);
}

QRectF SimpleTextItem::boundingRect() const
{
  return m_rect;
}

void SimpleTextItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  //painter->fillRect(boundingRect(), Qt::blue);
  if (!m_string.isEmpty())
  {
    painter->drawImage(QPointF{0, 0}, m_line);
  }
}

void SimpleTextItem::setFont(const QFont& f)
{
  m_font = std::move(f);
  m_font.setStyleStrategy(QFont::PreferAntialias);
  updateImpl();
}

void SimpleTextItem::setText(const QString& s)
{
  m_string = std::move(s);
  updateImpl();
}

void SimpleTextItem::setText(std::string_view s)
{
  m_string = QString::fromUtf8(s.data(), s.size());
  updateImpl();
}

void SimpleTextItem::setColor(const score::BrushSet& c)
{
  m_color = &c;
  updateImpl();
}

void SimpleTextItem::updateImpl()
{
  prepareGeometryChange();

  if (m_string.isEmpty())
  {
    m_rect = QRectF{};
    m_line = QImage{};
  }
  else
  {
    QTextLayout layout(m_string, m_font);
    layout.beginLayout();
    auto line = layout.createLine();
    layout.endLayout();

    m_rect = line.naturalTextRect();
    auto r = line.glyphRuns();

    if (r.size() > 0)
    {
      m_line = newImage(m_rect.width(), m_rect.height());

      QPainter p{&m_line};
      auto& skin = score::Skin::instance();

      if (m_color)
        p.setPen(m_color->pen1);
      p.setBrush(skin.NoBrush);
      p.drawGlyphRun(QPointF{0, 0}, r[0]);
    }
  }

  update();
}

QGraphicsTextButton::QGraphicsTextButton(QString text, QGraphicsItem* parent)
    : SimpleTextItem{score::Skin::instance().Base1.main, parent}
{
  setText(std::move(text));
}

void QGraphicsTextButton::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  pressed();
  event->accept();
}

void QGraphicsTextButton::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsTextButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

}
