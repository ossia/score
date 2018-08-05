#include <score/widgets/ControlWidgets.hpp>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionSlider>
#include <QPainter>
#include <ossia/network/dataspace/gain.hpp>
#include <cmath>

namespace Control
{
SCORE_LIB_BASE_EXPORT
const QPalette& transparentPalette()
{
  static QPalette p{[] {
    QPalette palette;
    palette.setBrush(QPalette::Background, Qt::transparent);
    return palette;
  }()};
  return p;
}

void ToggleButton::paintEvent(QPaintEvent* event)
{
  return QPushButton::paintEvent(event);
  /*
  QStyleOptionButton opt;
  opt.text = this->isChecked() ? alternatives[1] : alternatives[0];
  opt.state |= QStyle::State_On;
  opt.state |= QStyle::State_Enabled;
  opt.state |= this->isChecked() ? QStyle::State_Raised : QStyle::State_Sunken;
  initStyleOption(&opt);
  QPainter p{this};
  style()->drawControl(QStyle::CE_PushButton, &opt, &p, this);*/
}

void ValueSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(QString::number(value()));
}

void SpeedSlider::paintEvent(QPaintEvent*)
{
  paintWithText(QString{" x " + QString::number(double(value()) * 0.01)});
}

void VolumeSlider::paintEvent(QPaintEvent*)
{
  paintWithText(QString::number(ossia::detail::LinearGainToDecibels(value()), 'f', 1) + " dB");
}

void ValueDoubleSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(QString::number(min + value() * (max - min), 'f', 3));
}

void ValueLogDoubleSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(QString::number(std::exp2(min + value() * (max - min)), 'f', 3));
}

ComboSlider::ComboSlider(const QStringList& arr, QWidget* parent)
    : score::Slider{parent}, array{arr}
{
}

void ComboSlider::paintEvent(QPaintEvent* event)
{
  paintWithText(array[value()]);
}
}
