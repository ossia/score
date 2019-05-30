#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Inspector/Interval/SpeedSlider.hpp>

#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/StyleSheets.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QGridLayout>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

namespace Scenario
{

SpeedWidget::SpeedWidget(
    const Scenario::IntervalModel& model,
    const score::DocumentContext&,
    bool withButtons,
    bool showText,
    QWidget* parent)
    : QWidget{parent}, m_model{model}
{
  setObjectName("SpeedSlider");

  auto lay = new score::MarginLess<QGridLayout>{this};
  lay->setHorizontalSpacing(1);
  lay->setVerticalSpacing(1);

  auto setSpeedFun = [=](double val) {
    auto& dur = ((IntervalModel&)(m_model)).duration;
    auto s = double(val) / 100.0;
    if (dur.speed() != s)
    {
      dur.setSpeed(s);
    }
  };

  if (withButtons)
  {
    // Buttons
    int btn_col = 0;
    for (double factor : {0., 50., 100., 200., 500.})
    {
      auto pb = new QPushButton{"× " + QString::number(factor * 0.01), this};
      pb->setMinimumWidth(35);
      pb->setMaximumWidth(45);
      pb->setFlat(true);
      pb->setContentsMargins(0, 0, 0, 0);
      pb->setStyleSheet(
          "QPushButton { margin: 0px; padding: 0px; border:  1px solid #252930; "
          + score::ValueStylesheet + "}"
          + "QPushButton:hover { border: 1px solid #aaa;} ");

      connect(pb, &QPushButton::clicked, this, [=] { setSpeedFun(factor); });
      lay->addWidget(pb, 1, btn_col++, 1, 1);
    }
  }

  // Slider
  auto speedSlider = new score::SpeedSlider{this};
  speedSlider->showText = showText;
  speedSlider->setValue(m_model.duration.speed() * 100.);

  con(model.duration, &IntervalDurations::speedChanged, this, [=](double s) {
    double r = s * 100;
    if (!qFuzzyCompare(r, speedSlider->value()))
      speedSlider->setValue(r);
  });

  if(withButtons)
  {
    lay->addWidget(speedSlider, 0, 0, 1, 5);

    for (int i = 0; i < 5; i++)
      lay->setColumnStretch(i, 0);
    lay->setColumnStretch(5, 10);
  }
  else
  {
    lay->addWidget(speedSlider, 0, 0, 1, 1);
  }
  connect(speedSlider, &QSlider::valueChanged, this, setSpeedFun);
}

SpeedWidget::~SpeedWidget() {}
}


QSize Scenario::SpeedWidget::sizeHint() const
{
  auto sz = QWidget::sizeHint();
  sz.setWidth(200);
  return sz;
}
