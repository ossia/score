// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DoubleSlider.hpp"

#include <score/model/Skin.hpp>
#include <score/tools/Clamp.hpp>

#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::DoubleSlider)
namespace score
{
Slider::~Slider() = default;
DoubleSlider::~DoubleSlider() = default;

DoubleSlider::DoubleSlider(QWidget* parent) : Slider{Qt::Horizontal, parent}
{
  setMinimum(0);
  setMaximum(max + 1.);

  connect(this, &QSlider::valueChanged, this, [&](int val) {
    doubleValueChanged(double(val) / max);
  });
}

void DoubleSlider::setValue(double val)
{
  val = clamp(val, 0, 1);
  blockSignals(true);
  QSlider::setValue(val * max);
  blockSignals(false);
  doubleValueChanged(val);
}

double DoubleSlider::value() const
{
  return QSlider::value() / max;
}

Slider::Slider(Qt::Orientation ort, QWidget* widg) : QSlider{ort, widg}
{
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

Slider::Slider(QWidget* widg) : Slider{Qt::Horizontal, widg} {}

void Slider::paintEvent(QPaintEvent* e)
{
  QPainter p{this};
 paint(p);
}

void Slider::paint(QPainter& p)
{
  auto& skin = score::Skin::instance();
  const double min = minimum();
  const double max = maximum();
  double val = value();

  double ratio = 1. - (max - val) / (max - min);

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
  if(orientation() == Qt::Horizontal)
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

void Slider::paintWithText(const QString& s)
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
