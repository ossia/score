#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Inspector/Interval/SpeedSlider.hpp>

#include <score/tools/Bind.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/StyleSheets.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QGridLayout>
#include <QPushButton>
#include <QSlider>
#include <QWidget>
#include <QApplication>

namespace Scenario
{

SpeedWidget::SpeedWidget(bool withButtons, bool showText, QWidget* parent) : QWidget{parent}
{
  setObjectName("SpeedSlider");

  auto lay = new score::MarginLess<QGridLayout>{this};
  lay->setHorizontalSpacing(1);
  lay->setVerticalSpacing(1);

  auto setSpeedFun = [=](double) {
    if (m_model)
    {
      auto& dur = ((IntervalModel&)(*m_model)).duration;
      auto s = m_slider->speed();
      if (dur.speed() != s)
      {
        dur.setSpeed(s);
      }
    }
  };

  if (withButtons)
  {
    // Buttons
    int btn_col = 0;
    for (double factor : {0., 0.5, 1., 2., 5.})
    {
      auto pb = new QPushButton{"× " + QString::number(factor), this};

      pb->setMinimumWidth(35);
      pb->setMaximumWidth(45);
      pb->setFlat(true);
      pb->setContentsMargins(0, 0, 0, 0);

      connect(pb, &QPushButton::clicked, this, [=] { m_slider->setSpeed(factor); });
      lay->addWidget(pb, 1, btn_col++, 1, 1);
    }
  }

  // Slider
  m_slider = new score::SpeedSlider{this};
  m_slider->showText = showText;

  if (withButtons)
  {
    lay->addWidget(m_slider, 0, 0, 1, 5);

    for (int i = 0; i < 5; i++)
      lay->setColumnStretch(i, 0);
    lay->setColumnStretch(5, 10);
  }
  else
  {
    lay->addWidget(m_slider, 0, 0, 1, 1);
  }
  connect(m_slider, &score::SpeedSlider::valueChanged, this, setSpeedFun);
}

SpeedWidget::~SpeedWidget() { }

void SpeedWidget::setInterval(const IntervalModel& m)
{
  if (m_model)
  {
    QObject::disconnect(&m_model->duration, nullptr, this, nullptr);
  }

  m_model = &m;
  m_slider->setSpeed(m.duration.speed());

  con(m.duration, &IntervalDurations::speedChanged, this, [=](double s) {
    if (!qFuzzyCompare(s, m_slider->speed()))
      m_slider->setSpeed(s);
  });

  ::bind(m, IntervalModel::p_timeSignature{}, this, [=](bool t) {
    m_slider->tempo = t;
    m_slider->update();
  });
}

void SpeedWidget::unsetInterval()
{
  if (m_model)
  {
    QObject::disconnect(&m_model->duration, nullptr, this, nullptr);
  }

  m_model = nullptr;
}

QSize SpeedWidget::sizeHint() const
{
  auto sz = QWidget::sizeHint();
  sz.setWidth(200);
  return sz;
}
}
