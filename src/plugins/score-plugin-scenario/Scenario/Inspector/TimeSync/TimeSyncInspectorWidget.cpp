// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeSyncInspectorWidget.hpp"

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Commands/TimeSync/SetAutoTrigger.hpp>
#include <Scenario/Inspector/TimeSync/TriggerInspectorWidget.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/TextLabel.hpp>
#include <score/widgets/SpinBoxes.hpp>
#include <score/tools/Bind.hpp>
#include <Process/Dataflow/ControlWidgets.hpp>

#include <QCheckBox>
namespace Scenario
{
class TimeSignatureWidget : public QLineEdit
{
public:
  TimeSignatureWidget()
  {
    setContentsMargins(0, 0, 0, 0);
  }

  void setSignature(optional<Control::time_signature> t)
  {
    if(t)
    {
      setText(QString{"%1/%2"}.arg(t->upper).arg(t->lower));
    }
    else
    {
      setText(QString{"0/0"});
    }
  }

  optional<Control::time_signature> signature() const
  {
    return Control::get_time_signature(this->text().toStdString());
  }

};

TimeSyncInspectorWidget::TimeSyncInspectorWidget(
    const TimeSyncModel& object,
    const score::DocumentContext& ctx,
    QWidget* parent)
    : InspectorWidgetBase{object,
                          ctx,
                          parent,
                          tr("Sync (%1)").arg(object.metadata().getName())}
    , m_model{object}
{
  setObjectName("TimeSyncInspectorWidget");
  setParent(parent);

  // metadata
  m_metadata = new MetadataWidget{
      m_model.metadata(), ctx.commandStack, &m_model, this};

  m_metadata->setupConnections(m_model);

  addHeader(m_metadata);

  // default date
  m_date
      = new TextLabel{tr("Default date: ") + m_model.date().toString(), this};

  // Trigger
  m_autotrigger = new QCheckBox{tr("Auto-trigger")};
  m_autotrigger->setChecked(object.autotrigger());
  m_autotrigger->setWhatsThis(tr(R"_(Auto-trigger timesyncs are timesyncs which will
                                  directly restart their following floating scenario upon triggering.
                                  Else, triggering the timesync will stop the following subgraph and
                                  it will be necessary to trigger it again to restart it.
                                  This is only relevant for subgraphs not connected
                                  to the root of a score.)_"));
  m_autotrigger->setToolTip(m_autotrigger->whatsThis());
  connect(m_autotrigger, &QCheckBox::toggled,
          this, [&] (bool t) {
    if(t != object.autotrigger())
      CommandDispatcher<>{ctx.commandStack}.submit<Scenario::Command::SetAutoTrigger>(object, t);
  });
  connect(&object, &TimeSyncModel::autotriggerChanged,
          this, [&] (bool t) {
    if(t != m_autotrigger->isChecked())
      m_autotrigger->setChecked(t);
  });

  // Synchronization
#if defined(SCORE_MUSICAL)
  auto tempo = new score::SpinBox<double>{this};
  tempo->setRange(20., 400.);
  tempo->setValue(m_model.tempo());

  QObject::connect(
      tempo, qOverload<double>(&score::SpinBox<double>::valueChanged), this, [&ctx, &object] (double v) {
    CommandDispatcher<>{ctx.commandStack}.submit<Scenario::Command::SetTimeSyncTempo>(object, v);
      });

  QObject::connect(
      &m_model,
      &TimeSyncModel::tempoChanged,
      tempo,
      [=] (double t) {
    if(t != tempo->value())
      tempo->setValue(t);
  });


  auto sig = new TimeSignatureWidget;
  sig->setSignature(m_model.signature());

  QObject::connect(
      sig, &QLineEdit::editingFinished, this, [&ctx, &object, sig] {
    CommandDispatcher<>{ctx.commandStack}.submit<Scenario::Command::SetTimeSyncSignature>(object, Control::get_time_signature(sig->text().toStdString()));
      });

  QObject::connect(
      &m_model,
      &TimeSyncModel::signatureChanged,
      sig,
      [=] (auto s) {
    if(s != sig->signature())
      sig->setSignature(s);
  });
#endif
  m_trigwidg = new TriggerInspectorWidget{
      ctx,
      ctx.app.interfaces<Command::TriggerCommandFactoryList>(),
      m_model,
      this};
  updateAreaLayout({m_date, m_autotrigger,
                  #if defined(SCORE_MUSICAL)
                    tempo, sig,
                  #endif
                    new TextLabel{tr("Trigger")}, m_trigwidg});

  // display data
  updateDisplayedValues();

  con(m_model,
      &TimeSyncModel::dateChanged,
      this,
      &TimeSyncInspectorWidget::on_dateChanged);
}

void TimeSyncInspectorWidget::updateDisplayedValues()
{
  on_dateChanged(m_model.date());

  m_trigwidg->updateExpression(m_model.expression());
}

void TimeSyncInspectorWidget::on_dateChanged(const TimeVal& t)
{
  m_date->setText(tr("Default date: ") + t.toString());
}
}
