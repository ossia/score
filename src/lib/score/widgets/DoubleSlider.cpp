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
      setMinimumSize(18, 30);
      break;
    case Qt::Horizontal:
      setMinimumSize(30, 18);
      break;
  }
}

Slider::Slider(QWidget* widg) : Slider{Qt::Horizontal, widg} {}

void Slider::paintEvent(QPaintEvent*)
{
  QPainter p{this};
  paint(p);
}
void Slider::paint(QPainter& p)
{
  auto& skin = score::Skin::instance();
  double min = minimum();
  double max = maximum();
  double val = value();

  double ratio = 1. - (max - val) / (max - min);

  static constexpr auto round = 0;//1.5;
  p.setPen(QPen{QColor{"#62400a"},1});
  p.setBrush(skin.SliderBrush);
  QRect stroke_rect{rect().topLeft(),
                     QSize{rect().width()-1, rect().height()-1}};
  p.drawRoundedRect(stroke_rect, round, round);

  p.setPen(skin.TransparentPen);
  p.setBrush(skin.SliderExtBrush);

  if(orientation() == Qt::Horizontal)
  {
    const int current = int(ratio * width());
    p.drawRoundedRect(
        QRect{0, 0,
              current,height()
          }, round, round);

    if(current != 0)
    {
      p.setPen(skin.SliderLine);
      p.drawLine(QPoint{0, 0}, QPoint{current - 1, 0});
    }
  }
  else
  {
    const int h = int((1. - ratio) * height());

    p.drawRoundedRect(QRect{0, h, width(), height() - h},
                      round, round);

    if(int(h) != height())
    {
      p.setPen(skin.SliderLine);
      p.drawLine(QPoint{0, rect().bottomLeft().y()},QPoint{0, h});
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
