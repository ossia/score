// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TextItem.hpp"

#include <score/application/ApplicationContext.hpp>
#include <score/graphics/GraphicsItem.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QTextLayout>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::TextItem)
W_OBJECT_IMPL(score::SimpleTextItem)
W_OBJECT_IMPL(score::ClickableTextItem)

namespace score
{

TextItem::TextItem(QString text, QGraphicsItem* parent)
    : QGraphicsTextItem{std::move(text), parent}
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
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  static const bool vector_gui = score::AppContext().applicationSettings.vector_gui;
  if(vector_gui)
  {
    static const auto& skin = score::Skin::instance();
    if(m_color)
      painter->setPen(m_color->pen1);
    painter->setFont(m_font);
    painter->setBrush(skin.NoBrush);
    painter->drawText(QPointF{0, (float)m_line.height() - 2.}, m_string);
  }
  else if(!m_string.isEmpty())
  {
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->drawImage(QPointF{0, 0}, m_line);
  }
}

void SimpleTextItem::setFont(const QFont& f)
{
  m_font = f;
  m_font.setStyleStrategy(QFont::PreferAntialias);
  updateImpl();
}

const QString& SimpleTextItem::text() const noexcept
{
  return m_string;
}
void SimpleTextItem::setText(const QString& s)
{
  m_string = s;
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

  if(m_string.isEmpty())
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

    if(r.size() > 0)
    {
      m_line = newImage(m_rect.width(), m_rect.height());

      QPainter p{&m_line};
      auto& skin = score::Skin::instance();

      if(m_color)
        p.setPen(m_color->pen1);
      p.setBrush(skin.NoBrush);
      p.drawGlyphRun(QPointF{0, 0}, r[0]);
    }
  }

  update();
}

ClickableTextItem::ClickableTextItem(const score::BrushSet& brush, QGraphicsItem* parent)
    : score::SimpleTextItem{brush, parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void ClickableTextItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  clicked();
  event->accept();
}

void ClickableTextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}
}
