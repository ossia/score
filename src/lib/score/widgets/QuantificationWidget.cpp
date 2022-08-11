#include <score/widgets/QuantificationWidget.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QuantificationWidget)

namespace score
{
namespace
{
static int indexForQuantification(double d) noexcept
{
  if(d == -1.)
    return 0;
  if(d == 0.)
    return 1;
  if(d == 0.125)
    return 2;
  if(d == 0.250)
    return 3;
  if(d == 0.500)
    return 4;
  if(d == 1)
    return 5;
  if(d == 2)
    return 6;
  if(d == 4)
    return 7;
  if(d == 8)
    return 8;
  if(d == 16)
    return 9;
  if(d == 32)
    return 10;

  return 0;
}

static double quantificationForIndex(int d) noexcept
{
  switch(d)
  {
    case 0:
      return -1.;
    case 1:
      return 0.;
    case 2:
      return 0.125;
    case 3:
      return 0.250;
    case 4:
      return 0.500;
    case 5:
      return 1.;
    case 6:
      return 2.;
    case 7:
      return 4.;
    case 8:
      return 8.;
    case 9:
      return 16.;
    case 10:
      return 32.;
    default:
      return 0.;
  }
}
}

QuantificationWidget::QuantificationWidget(QWidget* parent)
    : QComboBox{parent}
{
  addItems(
      {tr("Parent"), tr("Free"), tr("8 bars"), tr("4 bars"), tr("2 bars"), tr("1 bar "),
       tr("1/2   "), tr("1/4   "), tr("1/8   "), tr("1/16  "), tr("1/32  ")});
  connect(this, qOverload<int>(&QComboBox::currentIndexChanged), this, [=](int idx) {
    quantificationChanged(quantificationForIndex(idx));
  });
}

double QuantificationWidget::quantification() const noexcept
{
  return quantificationForIndex(currentIndex());
}

void QuantificationWidget::setQuantification(double d)
{
  const auto idx = indexForQuantification(d);
  if(idx != currentIndex())
  {
    setCurrentIndex(idx);
    quantificationChanged(d);
  }
}

TimeSignatureWidget::TimeSignatureWidget()
{
  setContentsMargins(0, 0, 0, 0);
}

void TimeSignatureWidget::setSignature(std::optional<ossia::time_signature> t)
{
  if(t)
  {
    setText(QStringLiteral("%1/%2").arg(t->upper).arg(t->lower));
  }
  else
  {
    setText(QStringLiteral("0/0"));
  }
}

std::optional<ossia::time_signature> TimeSignatureWidget::signature() const
{
  return ossia::get_time_signature(this->text().toStdString());
}

}
