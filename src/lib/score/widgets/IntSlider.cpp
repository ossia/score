// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntSlider.hpp"

#include <score/model/Skin.hpp>
#include <score/tools/Clamp.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::IntSlider)
namespace score
{
IntSlider::~IntSlider() = default;

IntSlider::IntSlider(Qt::Orientation ort, QWidget* widg)
    : QWidget{widg}
    , m_orientation(ort)
{
  setFocusPolicy(Qt::FocusPolicy(style()->styleHint(QStyle::SH_Button_FocusPolicy)));
  QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Fixed, QSizePolicy::Slider);
  if(ort == Qt::Vertical)
    sp.transpose();
  setSizePolicy(sp);
  setAttribute(Qt::WA_WState_OwnSizePolicy, false);

  updateSkinMetrics();
  connect(
      &score::Skin::instance(), &score::Skin::changed, this,
      &IntSlider::updateSkinMetrics);
}

int IntSlider::skinExtent() const noexcept
{
  // One line of text, its border on both sides, and a pixel of air.
  return QFontMetrics{score::Skin::instance().SliderFont}.height()
         + 2 * qRound(m_borderWidth) + 2;
}

void IntSlider::updateSkinMetrics()
{
  auto& skin = score::Skin::instance();
  m_borderWidth = skin.SliderPen.width();

  update();

  // Only ever take back a minimum this class imposed. A caller that pinned
  // the control with setFixedSize has decided its size, and setting a smaller
  // minimum under it would collapse it to that minimum, since a plain QWidget
  // has no size hint of its own for the layout to fall back on.
  if(m_skinMinimum.isValid() && minimumSize() != m_skinMinimum)
    return;

  const int line = skinExtent();
  const int span = score::scaledPixels(30);

  m_skinMinimum
      = m_orientation == Qt::Vertical ? QSize{line, span} : QSize{span, line};
  setMinimumSize(m_skinMinimum);
  updateGeometry();
}

IntSlider::IntSlider(QWidget* widg)
    : IntSlider{Qt::Horizontal, widg}
{
}

void IntSlider::setValue(int val)
{
  if(m_value == val)
    return;

  m_value = clamp(val, m_min, m_max);
  repaint();
  valueChanged(m_value);
}

void IntSlider::updateValue(QPointF mousePos)
{
  if(m_orientation == Qt::Horizontal)
  {
    double clamped = clamp(mousePos.x(), m_borderWidth, width() - m_borderWidth);
    double ratio = (clamped - m_borderWidth) / (width() - 2 * m_borderWidth);
    m_value = m_min + (m_max - m_min) * ratio;
  }
  else
  {
    double clamped = clamp(mousePos.y(), m_borderWidth, height() - m_borderWidth);
    double ratio = (clamped - m_borderWidth) / (height() - 2 * m_borderWidth);
    m_value = m_min + (m_max - m_min) * (1. - ratio);
  }
  repaint();
  valueChanged(m_value);
}

void IntSlider::mousePressEvent(QMouseEvent* event)
{
  if(event->button() == Qt::MouseButton::RightButton)
  {
    createPopup(event->globalPosition().toPoint());
  }
  else if(event->button() == Qt::MouseButton::LeftButton)
  {
    updateValue(event->position());
  }
}

void IntSlider::mouseMoveEvent(QMouseEvent* event)
{
  updateValue(event->position());
  sliderMoved(m_value);
}

void IntSlider::mouseReleaseEvent(QMouseEvent* event)
{
  sliderReleased();
}

void IntSlider::mouseDoubleClickEvent(QMouseEvent* event)
{
  // See DoubleSlider::mouseDoubleClickEvent: reset to the declared default.
  if(m_max == m_min)
  {
    event->ignore();
    return;
  }
  m_value = clamp(m_init, m_min, m_max);
  repaint();
  valueChanged(m_value);
  sliderMoved(m_value);
  sliderReleased();
  event->accept();
}
void IntSlider::createPopup(QPoint pos)
{
  auto w = new QSpinBox();
  w->setWindowFlag(Qt::Tool);
  w->setWindowFlag(Qt::FramelessWindowHint);
  w->setValue(m_value);
  w->setRange(m_min, m_max);

  QObject::connect(w, SignalUtils::QSpinBox_valueChanged_int(), this, [this](int v) {
    this->setValue(v);
    sliderMoved(this->value());
  });
  w->show();
  w->move(pos.x(), pos.y());
  QTimer::singleShot(5, w, [w] { w->setFocus(); });
  QObject::connect(w, &QSpinBox::editingFinished, w, &QObject::deleteLater);
}
void IntSlider::paintEvent(QPaintEvent* e)
{
  QPainter p{this};
  paint(p);
}

void IntSlider::paint(QPainter& p)
{
  auto& skin = score::Skin::instance();

  double ratio = double(m_value - m_min) / double(m_max - m_min);
  p.setPen(skin.SliderPen);
  p.setBrush(skin.SliderBrush);
  const double penWidth = p.pen().width();
  p.drawRect(QRectF{
      QPointF{
          rect().topLeft().x() + penWidth / 2., rect().topLeft().y() + penWidth / 2.},
      QSizeF{rect().width() - penWidth, rect().height() - penWidth}});

  p.setPen(skin.TransparentPen);
  p.setBrush(skin.SliderInteriorBrush);

  const double interiorWidth = (double)width() - 2. * penWidth;
  const double interiorHeight = (double)height() - 2. * penWidth;
  if(m_orientation == Qt::Horizontal)
  {
    const double current = ratio * interiorWidth;
    p.drawRect(QRectF{QPointF{penWidth, penWidth}, QSizeF{current, interiorHeight}});

    if(!qFuzzyIsNull(current))
    {
      p.setPen(skin.SliderLine);
      const double linePenWidth = p.pen().width();
      p.drawLine(
          QPointF{penWidth, linePenWidth / 2.},
          QPointF{current + penWidth, linePenWidth / 2.});
    }
  }
  else
  {
    const double h = (1. - ratio) * interiorHeight;

    p.drawRect(QRectF{
        QPointF{penWidth, h + penWidth}, QSizeF{interiorWidth, (double)height() - h}});

    if(!qFuzzyCompare(h, interiorHeight))
    {
      p.setPen(skin.SliderLine);
      const double linePenWidth = p.pen().width();
      p.drawLine(
          QPointF{linePenWidth / 2., height() - penWidth},
          QPointF{linePenWidth / 2., h + penWidth});
    }
  }
}

void IntSlider::paintWithText(const QString& s)
{
  auto& skin = score::Skin::instance();

  QPainter p{this};
  paint(p);
  p.setPen(skin.SliderTextPen);
  p.setFont(skin.SliderFont);

  // Vertically centred: the box is only as tall as one line of the slider
  // font, so a top-aligned line would sit against the border.
  const double pad = score::scaledPixels(4);
  p.drawText(
      QRectF{pad, 0., width() - 4. * pad, (double)height()}, s,
      QTextOption(Qt::AlignLeft | Qt::AlignVCenter));
}

}
