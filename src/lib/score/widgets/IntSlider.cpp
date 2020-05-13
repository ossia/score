// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntSlider.hpp"

#include <score/model/Skin.hpp>
#include <score/tools/Clamp.hpp>

#include <QMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::IntSlider)
namespace score
{
IntSlider::~IntSlider() = default;

IntSlider::IntSlider(Qt::Orientation ort, QWidget* widg) :
  QWidget{widg},
  m_orientation(ort)
{
  setFocusPolicy(Qt::FocusPolicy(style()->styleHint(QStyle::SH_Button_FocusPolicy)));
  QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Fixed, QSizePolicy::Slider);
  if (ort == Qt::Vertical)
      sp.transpose();
  setSizePolicy(sp);
  setAttribute(Qt::WA_WState_OwnSizePolicy, false);

  auto& skin = score::Skin::instance();
  m_borderWidth = skin.SliderPen.width();

  switch (ort)
  {
    case Qt::Vertical:
      setMinimumSize(20, 30);
      break;
    case Qt::Horizontal:
      setMinimumSize(30, 20);
      break;
  }
}

IntSlider::IntSlider(QWidget* widg) : IntSlider{Qt::Horizontal, widg} {}

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
    double ratio =  (clamped - m_borderWidth) / (width() - 2 * m_borderWidth);
    m_value = m_min + (m_max - m_min) * ratio;

  }
  else
  {
    double clamped = clamp(mousePos.y(), m_borderWidth, height() - m_borderWidth);
    double ratio =  (clamped - m_borderWidth) / (height() - 2 * m_borderWidth);
    m_value = m_min + (m_max - m_min) * (1. - ratio);
  }
  repaint();
  valueChanged(m_value);
}

void IntSlider::mousePressEvent(QMouseEvent* event)
{
  updateValue(event->localPos());
}

void IntSlider::mouseMoveEvent(QMouseEvent* event)
{
  updateValue(event->localPos());
  sliderMoved(m_value);
}

void IntSlider::mouseReleaseEvent(QMouseEvent *event)
{
  sliderReleased();
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
  p.drawRect(QRectF{QPointF{rect().topLeft().x() + penWidth/2.,
                            rect().topLeft().y() + penWidth/2.},
                    QSizeF{rect().width()-penWidth, rect().height()-penWidth}});

  p.setPen(skin.TransparentPen);
  p.setBrush(skin.SliderInteriorBrush);

  const double interiorWidth = (double)width() - 2.* penWidth;
  const double interiorHeight = (double)height() - 2.* penWidth;
  if(m_orientation == Qt::Horizontal)
  {
    const double current = ratio * interiorWidth;
    p.drawRect(
        QRectF{QPointF{penWidth, penWidth},
              QSizeF{current, interiorHeight}
          });

    if(!qFuzzyIsNull(current))
    {
      p.setPen(skin.SliderLine);
      const double linePenWidth = p.pen().width();
      p.drawLine(QPointF{penWidth, linePenWidth/2.}, QPointF{current + penWidth, linePenWidth/2.});
    }
  }
  else
  {
    const double h = (1. - ratio) * interiorHeight;

    p.drawRect(QRectF{QPointF{penWidth, h + penWidth},
                      QSizeF{interiorWidth, (double)height()- h}});

    if(!qFuzzyCompare(h, interiorHeight))
    {
      p.setPen(skin.SliderLine);
      const double linePenWidth = p.pen().width();
      p.drawLine(QPointF{linePenWidth/2., height() - penWidth},
                 QPointF{linePenWidth/2., h + penWidth});
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
  p.drawText(
      QRectF{4., 2., (width() - 16.), height() - 4.},
      s,
      QTextOption(Qt::AlignLeft));
}

}
