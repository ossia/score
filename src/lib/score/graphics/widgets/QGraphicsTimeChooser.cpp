#include "QGraphicsTimeChooser.hpp"

#include <ossia/detail/algorithms.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsTimeChooser)
namespace score
{

static constexpr float QGraphicsTimeChooser_ratios[] = {
    1.,       1. / 2., 1. / 4., 1. / 8.,  1. / 16., 1. / 32.,
    1. / 64., 3. / 4., 3. / 8., 3. / 16., 3. / 32., 3. / 64.,
};
QGraphicsTimeChooser::QGraphicsTimeChooser(QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , knob{this}
    , combo{this}
    , toggle{"Free", "Sync", this}
{
  this->setFlag(ItemHasNoContents, true);
  connect(&knob, &QGraphicsKnob::sliderMoved, this, &QGraphicsTimeChooser::sliderMoved);
  connect(
      &knob, &QGraphicsKnob::sliderReleased, this,
      &QGraphicsTimeChooser::sliderReleased);
  connect(
      &combo, &QGraphicsCombo::sliderMoved, this, &QGraphicsTimeChooser::sliderMoved);
  connect(
      &combo, &QGraphicsCombo::sliderReleased, this,
      &QGraphicsTimeChooser::sliderReleased);

  knob.setRange(0., 1.);
  combo.array = QStringList{
      "Whole", "Half",        "4th",        "8th",        "16th",        "32th",
      "64th",  "Dotted half", "Dotted 4th", "Dotted 8th", "Dotted 16th", "Dotted 32th",
  };
  combo.setVisible(false);

  const double delta_x = 5.;
  const double delta_y = 0.;
  const auto knob_rect = knob.boundingRect();
  const auto combo_rect = combo.boundingRect();
  const auto tog_rect = toggle.boundingRect();
  const double combo_x = 0.;
  const double knob_x = combo_rect.width() / 2. - knob_rect.width() / 2.;
  const double tog_x = combo_rect.width() / 2. - tog_rect.width() / 2.;

  const double knob_y = 0.;
  const double combo_y = knob_rect.height() / 2. - combo_rect.height() / 2.;
  combo.setPos(delta_x + combo_x, delta_y + combo_y);
  knob.setPos(delta_x + knob_x, delta_y + knob_y);
  auto h
      = std::max(knob.boundingRect().height() + 2., combo.boundingRect().height() + 2.);
  toggle.setPos(delta_x + tog_x, delta_y + h);
  connect(&toggle, &QGraphicsToggle::toggled, this, &QGraphicsTimeChooser::syncChanged);
}

QGraphicsTimeChooser::~QGraphicsTimeChooser() { }

void QGraphicsTimeChooser::syncChanged(bool sync)
{
  knob.setVisible(!sync);
  combo.setVisible(sync);
  toggle.setState(sync);
  sliderMoved();
  sliderReleased();
}

void QGraphicsTimeChooser::setValue(ossia::vec2f v)
{
  const bool sync = v[1] != 0.;
  knob.setVisible(!sync);
  combo.setVisible(sync);
  toggle.setState(sync);

  if(!sync)
  {
    knob.setValue(v[0]);
  }
  else
  {
    auto it = ossia::find(QGraphicsTimeChooser_ratios, v[0]);
    if(it != std::end(QGraphicsTimeChooser_ratios))
    {
      combo.setValue(std::distance(std::begin(QGraphicsTimeChooser_ratios), it));
    }
    else
    {
      combo.setValue(2);
    }
  }
}

ossia::vec2f QGraphicsTimeChooser::value() const noexcept
{
  if(!toggle.state())
    return {(float)knob.value(), 0.};
  else
    return {QGraphicsTimeChooser_ratios[combo.value()], 1.};
}

void QGraphicsTimeChooser::setExecutionValue(ossia::vec2f v) { }

void QGraphicsTimeChooser::resetExecution()
{
  knob.resetExecution();
}

QRectF QGraphicsTimeChooser::boundingRect() const
{
  return {0, 0, 60, 60};
}

void QGraphicsTimeChooser::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
}

}
