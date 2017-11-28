// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "IntervalInspectorWidget.hpp"
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Scenario/Inspector/Interval/Widgets/DurationSectionWidget.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/Separator.hpp>
#include <score/widgets/TextLabel.hpp>
#include <score/widgets/MarginLess.hpp>

namespace Scenario
{
IntervalInspectorWidget::IntervalInspectorWidget(
    const Inspector::InspectorWidgetList& widg,
    const IntervalModel& object,
    std::unique_ptr<IntervalInspectorDelegate> del,
    const score::DocumentContext& ctx,
    QWidget* parent)
    : InspectorWidgetBase{object, ctx, parent, tabName()}
    , m_widgetList{widg}
    , m_model{object}
    , m_delegate{std::move(del)}
{
  using namespace score;
  using namespace score::IDocument;
  setObjectName("Interval");

  ////// HEADER
  // metadata
  m_metadata = new MetadataWidget{m_model.metadata(), ctx.commandStack,
                                  &m_model, this};

  m_metadata->setupConnections(m_model);

  addHeader(m_metadata);

  {
    auto speedWidg = new QWidget{this};
    auto lay = new score::MarginLess<QVBoxLayout>{speedWidg};

    // Label
    auto speedLab = new TextLabel{
        "Speed x" + QString::number(m_model.duration.executionSpeed())};
    lay->addWidget(speedLab);

    auto speedLay = new score::MarginLess<QGridLayout>;
    lay->addLayout(speedLay);
    speedLay->setHorizontalSpacing(0);
    speedLay->setVerticalSpacing(0);

    auto setSpeedFun = [=](int val) {
      auto& dur = ((IntervalModel&)(m_model)).duration;
      auto s = double(val) / 100.0;
      if (dur.executionSpeed() != s)
      {
        dur.setExecutionSpeed(s);
      }
    };
    // Buttons
    int btn_col = 0;
    for (int factor : {0, 50, 100, 200, 500})
    {
      auto pb
          = new QPushButton{"x " + QString::number(factor / 100.0), speedWidg};
      pb->setMinimumWidth(35);
      pb->setMaximumWidth(35);
      pb->setContentsMargins(0, 0, 0, 0);
      pb->setStyleSheet(QStringLiteral("QPushButton { margin: 0px; padding: 0px; }"));

      connect(pb, &QPushButton::clicked, this, [=] { setSpeedFun(factor); });
      speedLay->addWidget(pb, 1, btn_col++, 1, 1);
    }

    // Slider
    auto speedSlider = new QSlider{Qt::Horizontal};
    speedSlider->setTickInterval(100);
    speedSlider->setMinimum(-100);
    speedSlider->setMaximum(500);
    speedSlider->setValue(m_model.duration.executionSpeed() * 100.);
    con(m_model.duration, &IntervalDurations::executionSpeedChanged, this,
        [=](double s) {
          double r = s * 100;
          speedLab->setText("Speed x" + QString::number(s));
          if (r != speedSlider->value())
            speedSlider->setValue(r);
        });

    speedLay->addWidget(speedSlider, 1, btn_col, 1, 1);

    for (int i = 0; i < 5; i++)
      speedLay->setColumnStretch(i, 0);
    speedLay->setColumnStretch(5, 10);
    connect(speedSlider, &QSlider::valueChanged, this, setSpeedFun);

    m_properties.push_back(speedWidg);
  }

  m_delegate->addWidgets_pre(m_properties, this);

  ////// BODY
  auto setAsDisplayedInterval = new QPushButton{tr("Full view"), this};
  connect(setAsDisplayedInterval, &QPushButton::clicked, this, [this] {
    auto& base = get<ScenarioDocumentPresenter>(*documentFromObject(m_model));

    base.setDisplayedInterval(model());
  });

  // Transport
  {
    auto transportWid = new QWidget{this};
    auto transportLay = new score::MarginLess<QHBoxLayout>{transportWid};

    auto scenar = dynamic_cast<Scenario::ScenarioInterface*>(m_model.parent());
    SCORE_ASSERT(scenar);
    transportLay->addStretch(1);

    transportLay->addWidget(setAsDisplayedInterval);

    transportLay->addStretch(1);

    m_properties.push_back(transportWid);
  }

  // Separator
  m_properties.push_back(new score::HSeparator{this});

  // Durations
  auto& ctrl
      = ctx.app.guiApplicationPlugin<ScenarioApplicationPlugin>();
  m_durationSection
      = new DurationWidget{ctrl.editionSettings(), *m_delegate, this};
  m_properties.push_back(m_durationSection);

  updateDisplayedValues();

  m_delegate->addWidgets_post(m_properties, this);

  // Display data
  updateAreaLayout(m_properties);
}

IntervalInspectorWidget::~IntervalInspectorWidget() = default;

IntervalModel& IntervalInspectorWidget::model() const
{
  return const_cast<IntervalModel&>(m_model);
}

QString IntervalInspectorWidget::tabName()
{
  return tr("Interval");
}

void IntervalInspectorWidget::updateDisplayedValues()
{
  m_delegate->updateElements();
}

}
