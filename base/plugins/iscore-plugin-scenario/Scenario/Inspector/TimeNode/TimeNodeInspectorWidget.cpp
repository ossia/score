// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeNodeInspectorWidget.hpp"

#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Commands/TimeNode/SplitTimeNode.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Inspector/Event/EventInspectorWidget.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>
#include <Scenario/Inspector/TimeNode/TriggerInspectorWidget.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/widgets/Separator.hpp>

#include <Inspector/InspectorWidgetBase.hpp>
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
#include <Scenario/Process/ScenarioInterface.hpp>
#include <algorithm>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/TextLabel.hpp>

namespace Scenario
{
TimeNodeInspectorWidget::TimeNodeInspectorWidget(
    const TimeNodeModel& object,
    const iscore::DocumentContext& ctx,
    QWidget* parent)
    : InspectorWidgetBase{object, ctx, parent}, m_model{object}
{
  setObjectName("TimeNodeInspectorWidget");
  setParent(parent);

  // metadata
  m_metadata = new MetadataWidget{m_model.metadata(), ctx.commandStack,
                                  &m_model, this};

  m_metadata->setupConnections(m_model);

  addHeader(m_metadata);

  // default date
  m_date = new TextLabel{tr("Default date: ") + m_model.date().toString(), this};

  // Trigger
  m_trigwidg = new TriggerInspectorWidget{
      ctx, ctx.app.interfaces<Command::TriggerCommandFactoryList>(),
      m_model, this};
  updateAreaLayout({m_date, new iscore::HSeparator{this}, new TextLabel{tr("Trigger")}, m_trigwidg});

  // display data
  updateDisplayedValues();

  con(m_model, &TimeNodeModel::dateChanged, this,
      &TimeNodeInspectorWidget::on_dateChanged);
}

QString TimeNodeInspectorWidget::tabName()
{
  return tr("TimeNode");
}

void TimeNodeInspectorWidget::updateDisplayedValues()
{
  on_dateChanged(m_model.date());

  m_trigwidg->updateExpression(m_model.expression());
}

void TimeNodeInspectorWidget::on_dateChanged(const TimeVal& t)
{
  m_date->setText(tr("Default date: ") + t.toString());
}
}
