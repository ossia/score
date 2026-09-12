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
  auto& skin = score::Skin::instance();
  setFont(skin.Medium8Pt);

  // Follow the skin: the glyphs are cached into m_line, so without this the
  // label keeps the font it was built with until the item is recreated, which
  // is what made the font settings look like they needed a restart.
  QObject::connect(&skin, &score::Skin::changed, this, [this] { updateImpl(); });
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
    if(m_font)
      painter->setFont(*m_font);
    painter->setBrush(skin.NoBrush);
    painter->drawText(QPointF{0, (float)m_rect.height() - 2.}, m_string);
  }
  else if(!m_string.isEmpty())
  {
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->drawImage(QPointF{0, 0}, m_line);
  }
}

void SimpleTextItem::setFont(const QFont& f)
{
  // A reference, not a copy, so that a skin change re-renders this label.
  //
  // Note also what is *not* here: the style strategy is left alone. Forcing
  // PreferAntialias turns on subpixel glyph positioning, and these items are
  // laid out at fractional device offsets (measured: x fractions of .11 to
  // .81), so a pixel font's 1 px stems get split across two columns and the
  // label looks smeared. NoAntialias is what makes Qt snap the glyph origin
  // to a whole pixel.
  m_font = &f;
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
    QTextLayout layout(m_string, m_font ? *m_font : QFont{});
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
