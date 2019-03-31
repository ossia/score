// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeSyncInspectorWidget.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Scenario/Commands/TimeSync/SplitTimeSync.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Inspector/Event/EventInspectorWidget.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/TimeSync/TriggerInspectorWidget.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/Todo.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/Separator.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QApplication>
#include <QBoxLayout>
#include <QColor>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include <algorithm>

namespace Scenario
{
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
  m_trigwidg = new TriggerInspectorWidget{
      ctx,
      ctx.app.interfaces<Command::TriggerCommandFactoryList>(),
      m_model,
      this};
  updateAreaLayout({m_date, new TextLabel{tr("Trigger")}, m_trigwidg});

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
