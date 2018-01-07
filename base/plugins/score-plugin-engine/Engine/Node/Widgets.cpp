#include "Widgets.hpp"
#include "Layer.hpp"

namespace Control
{

const QPalette& transparentPalette()
{
  static QPalette p{[] {
      QPalette palette;
      palette.setBrush(QPalette::Background, Qt::transparent);
      return palette;
                    }()};
  return p;
}

ILayerView::~ILayerView()
{

}

void RectItem::setRect(QRectF r) { prepareGeometryChange(); m_rect = r; }

void RectItem::setHighlight(bool b)
{
  m_highlight = b;
  update();
}

QRectF RectItem::boundingRect() const
{
  return m_rect;
}

void RectItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  static const auto& style = ScenarioStyle::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setBrush(style.TransparentBrush);
  painter->setPen(!m_highlight ? style.MinimapPen : style.MiniScenarioPen);
  painter->drawRoundedRect(m_rect, 5, 5);
  painter->setRenderHint(QPainter::Antialiasing, false);
}

void RectItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  this->setZValue(10);
}

void RectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  this->setZValue(0);
}
void RectItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}
void RectItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}
void RectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  emit clicked();
  event->accept();
}

EmptyRectItem::EmptyRectItem(QGraphicsItem* parent):
  QGraphicsItem{parent}
{
  this->setFlag(ItemHasNoContents, true);
}
void EmptyRectItem::setRect(QRectF r) { prepareGeometryChange(); m_rect = r; }

QRectF EmptyRectItem::boundingRect() const
{
  return m_rect;
}

void EmptyRectItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
}

void EmptyRectItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  this->setZValue(10);
}

void EmptyRectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  this->setZValue(0);
}
void EmptyRectItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}
void EmptyRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}
void EmptyRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  emit clicked();
  event->accept();
}

void ToggleButton::paintEvent(QPaintEvent* event)
{
  return QPushButton::paintEvent(event);
  QStyleOptionButton opt;
  opt.text = this->isChecked() ? alternatives[1] : alternatives[0];
  opt.state |= QStyle::State_On;
  opt.state |= QStyle::State_Enabled;
  opt.state |= this->isChecked() ? QStyle::State_Raised : QStyle::State_Sunken;
  initStyleOption(&opt);
  QPainter p{this};
  style()->drawControl(QStyle::CE_PushButton, &opt, &p, this);
}

void ValueSlider::paintEvent(QPaintEvent* event)
{
  QSlider::paintEvent(event);
  QStyleOptionSlider opt;
  initStyleOption(&opt);
  QPainter p{this};
  style()->drawItemText(&p, this->rect(), 0, opt.palette, true, QString::number(value()));
}

void ValueDoubleSlider::paintEvent(QPaintEvent* event)
{
  QSlider::paintEvent(event);
  QStyleOptionSlider opt;
  initStyleOption(&opt);
  QPainter p{this};
  style()->drawItemText(&p, this->rect(), 0, opt.palette, true, QString::number(min + value() * (max - min), 'f', 3));
}

void ValueLogDoubleSlider::paintEvent(QPaintEvent* event)
{
  QSlider::paintEvent(event);
  QStyleOptionSlider opt;
  initStyleOption(&opt);
  QPainter p{this};
  style()->drawItemText(
        &p, this->rect(), 0, opt.palette, true,
        QString::number(std::exp2(min + value() * (max - min)), 'f', 3));
}

ComboSlider::ComboSlider(const QStringList& arr, QWidget* parent):
  QSlider{parent},
  array{arr}
{

}

void ComboSlider::paintEvent(QPaintEvent* event)
{
  QSlider::paintEvent(event);
  QStyleOptionSlider opt;
  initStyleOption(&opt);
  QPainter p{this};
  style()->drawItemText(&p, this->rect(), 0, opt.palette, true, array[value()]);
}

}
