// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DoubleSlider.hpp"
#include <QPainter>
#include <wobjectimpl.h>
W_OBJECT_IMPL(score::DoubleSlider)
namespace score
{
AbsoluteSliderStyle::~AbsoluteSliderStyle() = default;
Slider::~Slider() = default;
DoubleSlider::~DoubleSlider() = default;

AbsoluteSliderStyle* AbsoluteSliderStyle::instance() noexcept
{
  static AbsoluteSliderStyle style;
  return &style;
}


int AbsoluteSliderStyle::styleHint(
      QStyle::StyleHint hint
      , const QStyleOption* option
      , const QWidget* widget
      , QStyleHintReturn* returnData) const
{
  switch(hint)
  {
    case QStyle::SH_Slider_AbsoluteSetButtons:
      return Qt::AllButtons;
    default:
      return QProxyStyle::styleHint(hint, option, widget, returnData);
  }
}

DoubleSlider::DoubleSlider(QWidget* parent)
  : Slider{Qt::Horizontal, parent}
{
  setMinimum(0);
  setMaximum(max + 1);

  connect(this, &QSlider::valueChanged, this, [&](int val) {
    valueChanged(double(val) / max);
  });
}

void DoubleSlider::setValue(double val)
{
  val = clamp(val, 0, 1);
  blockSignals(true);
  QSlider::setValue(val * max);
  blockSignals(false);
}

double DoubleSlider::value() const
{
  return QSlider::value() / max;
}

Slider::Slider(Qt::Orientation ort, QWidget* widg)
  : QSlider{ort, widg}
{
  setStyle(AbsoluteSliderStyle::instance());
}

Slider::Slider(QWidget* widg)
  : Slider{Qt::Horizontal, widg}
{
}

void Slider::paintEvent(QPaintEvent*)
{
  QPainter p{this};
  paint(p);
}

void Slider::paint(QPainter& p)
{
  double min = minimum();
  double max = maximum();
  double val = value();

  double ratio = 1. - (max - val) / (max - min);

  static constexpr auto round = 1.5;
  p.setPen(Qt::transparent);
  p.setBrush(QColor("#12171a"));
  p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), round, round);

  p.setBrush(QColor("#666"));
  p.drawRoundedRect(QRect{3, 3, int(ratio * (width() - 6)), height() - 6}, round, round);
}

void Slider::paintWithText(const QString& s)
{
  QPainter p{this};
  paint(p);

  p.setPen(QColor("silver"));
  p.setFont(QFont("Ubuntu", 8));
  p.drawText(QRect{13, 3, (width() - 16), height() - 6}, s);
}

}
