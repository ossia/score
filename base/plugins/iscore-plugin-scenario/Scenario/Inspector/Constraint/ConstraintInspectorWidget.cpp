#include <Inspector/InspectorWidgetList.hpp>
#include <Process/Process.hpp>
#include <QBoxLayout>
#include <QCheckBox>
#include <QColor>
#include <QFormLayout>
#include <QLabel>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QToolButton>
#include <QWidget>
#include <QtGlobal>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/widgets/Separator.hpp>
#include <iscore/widgets/TextLabel.hpp>
#include <utility>

#include "ConstraintInspectorWidget.hpp"
#include "Widgets/DurationSectionWidget.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Inspector/Constraint/Widgets/ProcessTabWidget.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SpinBoxes.hpp>

namespace Scenario
{
ConstraintInspectorWidget::ConstraintInspectorWidget(
    const Inspector::InspectorWidgetList& widg,
    const Process::ProcessFactoryList& pl,
    const ConstraintModel& object,
    std::unique_ptr<ConstraintInspectorDelegate>
        del,
    const iscore::DocumentContext& ctx,
    QWidget* parent)
    : InspectorWidgetBase{object, ctx, parent}
    , m_widgetList{widg}
    , m_processList{pl}
    , m_model{object}
    , m_delegate{std::move(del)}
{
  using namespace iscore;
  using namespace iscore::IDocument;
  setObjectName("Constraint");

  ////// HEADER
  // metadata
  m_metadata = new MetadataWidget{m_model.metadata(), ctx.commandStack,
                                  &m_model, this};

  m_metadata->setupConnections(m_model);

  addHeader(m_metadata);

  {
    auto speedWidg = new QWidget{this};
    auto lay = new iscore::MarginLess<QVBoxLayout>{speedWidg};

    // Label
    auto speedLab = new TextLabel{
        "Speed x" + QString::number(m_model.duration.executionSpeed())};
    lay->addWidget(speedLab);

    auto speedLay = new iscore::MarginLess<QGridLayout>;
    lay->addLayout(speedLay);
    speedLay->setHorizontalSpacing(0);
    speedLay->setVerticalSpacing(0);

    auto setSpeedFun = [=](int val) {
      auto& dur = ((ConstraintModel&)(m_model)).duration;
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
    con(m_model.duration, &ConstraintDurations::executionSpeedChanged, this,
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
  auto setAsDisplayedConstraint = new QPushButton{tr("Full view"), this};
  connect(setAsDisplayedConstraint, &QPushButton::clicked, this, [this] {
    auto& base = get<ScenarioDocumentPresenter>(*documentFromObject(m_model));

    base.setDisplayedConstraint(model());
  });

  // Transport
  {
    auto transportWid = new QWidget{this};
    auto transportLay = new iscore::MarginLess<QHBoxLayout>{transportWid};

    auto scenar = dynamic_cast<Scenario::ScenarioInterface*>(m_model.parent());
    ISCORE_ASSERT(scenar);
    transportLay->addStretch(1);

    auto sst = m_model.startState();
    {
      auto btn = SelectionButton::make(
          tr("Start State"), &scenar->state(sst), selectionDispatcher(), this);
      transportLay->addWidget(btn);
    }
    transportLay->addWidget(setAsDisplayedConstraint);

    auto est = m_model.endState();
    {
      auto btn = SelectionButton::make(
          tr("End State"), &scenar->state(est), selectionDispatcher(), this);
      transportLay->addWidget(btn);
    }
    transportLay->addStretch(1);

    m_properties.push_back(transportWid);
  }

  // Separator
  m_properties.push_back(new Inspector::HSeparator{this});

  // Durations
  auto& ctrl
      = ctx.app.guiApplicationPlugin<ScenarioApplicationPlugin>();
  m_durationSection
      = new DurationWidget{ctrl.editionSettings(), *m_delegate, this};
  m_properties.push_back(m_durationSection);

  // Separator
  m_properties.push_back(new Inspector::HSeparator{this});

  // Processes
  m_processesTabPage = new ProcessTabWidget{*this, this};
  m_properties.push_back(m_processesTabPage);

  // Constraint interface
  model()
      .processes.added
      .connect<ConstraintInspectorWidget, &ConstraintInspectorWidget::on_processCreated>(
          this);
  model()
      .processes.removed
      .connect<ConstraintInspectorWidget, &ConstraintInspectorWidget::on_processRemoved>(
          this);
  model()
      .processes.orderChanged
      .connect<ConstraintInspectorWidget, &ConstraintInspectorWidget::on_orderChanged>(
          this);

  updateDisplayedValues();

  m_delegate->addWidgets_post(m_properties, this);

  // Display data
  updateAreaLayout(m_properties);
}

ConstraintInspectorWidget::~ConstraintInspectorWidget() = default;

ConstraintModel& ConstraintInspectorWidget::model() const
{
  return const_cast<ConstraintModel&>(m_model);
}

QString ConstraintInspectorWidget::tabName()
{
  return tr("Constraint");
}
void ConstraintInspectorWidget::updateDisplayedValues()
{
  // Cleanup the widgets
  m_processesTabPage->updateDisplayedValues();

  m_delegate->updateElements();
}

void ConstraintInspectorWidget::on_processCreated(const Process::ProcessModel&)
{
  // OPTIMIZEME
  // Note: this is put in the event loop because when creating a process,
  // this signal is emitted before the process could be put as the front layer.
  // hence the incorrect process is expanded.
  // TODO we should also update when a new process is displayed in a new slot...
  QTimer::singleShot(0, [this] {
      if(m_processesTabPage)
          m_processesTabPage->updateDisplayedValues();
  });
}

void ConstraintInspectorWidget::on_processRemoved(const Process::ProcessModel&)
{
  // OPTIMIZEME
    QTimer::singleShot(0, [this] {
        if(m_processesTabPage)
            m_processesTabPage->updateDisplayedValues();
    });
}

void ConstraintInspectorWidget::on_orderChanged()
{
  // OPTIMIZEME
    QTimer::singleShot(0, [this] {
        if(m_processesTabPage)
            m_processesTabPage->updateDisplayedValues();
    });
}

}
