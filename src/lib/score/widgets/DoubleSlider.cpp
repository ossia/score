// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DoubleSlider.hpp"

#include "DoubleSpinBox.hpp"

#include <score/model/Skin.hpp>
#include <score/tools/Clamp.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionSlider>
#include <QTimer>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::DoubleSlider)
namespace score
{
DoubleSlider::~DoubleSlider() = default;

DoubleSlider::DoubleSlider(Qt::Orientation ort, QWidget* widg)
    : QWidget{widg}
    , m_orientation(ort)
{
  setFocusPolicy(Qt::FocusPolicy(style()->styleHint(QStyle::SH_Button_FocusPolicy)));
  QSizePolicy sp(QSizePolicy::Ignored, QSizePolicy::Fixed, QSizePolicy::Slider);
  if(ort == Qt::Vertical)
    sp.transpose();
  setSizePolicy(sp);
  setAttribute(Qt::WA_WState_OwnSizePolicy, false);

  updateSkinMetrics();
  connect(
      &score::Skin::instance(), &score::Skin::changed, this,
      &DoubleSlider::updateSkinMetrics);
}

void DoubleSlider::updateSkinMetrics()
{
  auto& skin = score::Skin::instance();
  if(!m_borderOverridden)
    m_borderWidth = skin.SliderPen.width();

  // One line of text, its border on both sides, and a pixel of air.
  const int line
      = QFontMetrics{skin.SliderFont}.height() + 2 * qRound(m_borderWidth) + 2;
  const int span = score::scaledPixels(30);

  switch(m_orientation)
  {
    case Qt::Vertical:
      setMinimumSize(line, span);
      break;
    case Qt::Horizontal:
      setMinimumSize(span, line);
      break;
  }
  updateGeometry();
  update();
}

DoubleSlider::DoubleSlider(QWidget* widg)
    : DoubleSlider{Qt::Horizontal, widg}
{
}

void DoubleSlider::setValue(double val)
{
  m_value = clamp(val, 0, 1);
  valueChanged(m_value);
  repaint();
}

double DoubleSlider::map(double v) const
{
  return min + v * (max - min);
}

double DoubleSlider::unmap(double v) const
{
  if(max == min)
    return 0.;
  return (v - min) / (max - min);
}

void DoubleSlider::updateValue(QPointF mousePos)
{
  if(m_orientation == Qt::Horizontal)
  {
    double clamped = clamp(mousePos.x(), m_borderWidth, width() - m_borderWidth);
    m_value = (clamped - m_borderWidth) / (width() - 2 * m_borderWidth);
  }
  else
  {
    double clamped = clamp(mousePos.y(), m_borderWidth, height() - m_borderWidth);
    m_value = 1 - (clamped - m_borderWidth) / (height() - 2 * m_borderWidth);
  }

  repaint();
  valueChanged(m_value);
}

void DoubleSlider::mousePressEvent(QMouseEvent* event)
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

void DoubleSlider::mouseMoveEvent(QMouseEvent* event)
{
  updateValue(event->position());
  sliderMoved(m_value);
}

void DoubleSlider::mouseReleaseEvent(QMouseEvent* event)
{
  sliderReleased();
}

void DoubleSlider::mouseDoubleClickEvent(QMouseEvent* event)
{
  // Same gesture as the graphics-view controls: back to the default the
  // process declared. The press that opened the double click already moved the
  // value under the cursor, so this has to overwrite it.
  if(max == min)
  {
    // No domain was ever set on this slider: there is no default to go to.
    event->ignore();
    return;
  }
  m_value = clamp(unmap(init), 0., 1.);
  repaint();
  valueChanged(m_value);
  sliderMoved(m_value);
  sliderReleased();
  event->accept();
}
void DoubleSlider::createPopup(QPoint pos)
{
  auto w = new score::DoubleSpinboxWithEnter;
  w->setWindowFlag(Qt::Tool);
  w->setWindowFlag(Qt::FramelessWindowHint);
  w->setRange(map(0), map(1));
  w->setValue(map(m_value));
  w->setDecimals(3);
  QObject::connect(
      w, SignalUtils::QDoubleSpinBox_valueChanged_double(), this, [this](double v) {
    this->setValue(this->unmap(v));
    sliderMoved(this->value());
  });
  w->show();
  w->move(pos.x(), pos.y());
  QTimer::singleShot(5, w, [w] { w->setFocus(); });
  QObject::connect(
      w, &DoubleSpinboxWithEnter::editingFinished, w, &QObject::deleteLater);
}
void DoubleSlider::setRange(double min, double max, double init) noexcept
{
  this->min = min;
  this->max = max;
  this->init = init;
  update();
}
void DoubleSlider::paintEvent(QPaintEvent* e)
{
  QPainter p{this};
  paint(p);
}

void DoubleSlider::paint(QPainter& p)
{
  auto& skin = score::Skin::instance();

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
    const double current = m_value * interiorWidth;
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
    const double h = (1. - m_value) * interiorHeight;

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

void DoubleSlider::paintWithText(const QString& s)
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
