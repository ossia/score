// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Commands/MoveBaseEvent.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorWidget.hpp>
#include <Scenario/Inspector/TimeSync/TriggerInspectorWidget.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <score/tools/std/Optional.hpp>

#include <QByteArray>
#include <QDataStream>
#include <QLabel>
#include <QObject>
#include <QtGlobal>
#include <score/application/ApplicationContext.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QString>

#include "BaseIntervalInspectorDelegate.hpp"
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegate.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <Process/TimeValueSerialization.hpp>

namespace Scenario
{
BaseIntervalInspectorDelegate::BaseIntervalInspectorDelegate(
    const IntervalModel& cst)
    : IntervalInspectorDelegate{cst}
{
}

void BaseIntervalInspectorDelegate::updateElements()
{
  auto& scenario = *safe_cast<BaseScenario*>(m_model.parent());
  auto& tn = endTimeSync(m_model, scenario);
  m_triggerLine->updateExpression(tn.expression());
}

void BaseIntervalInspectorDelegate::addWidgets_pre(
    std::vector<QWidget*>& widgets, IntervalInspectorWidget* parent)
{
  auto& scenario = *safe_cast<BaseScenario*>(m_model.parent());
  auto& ctx = score::IDocument::documentContext(scenario);
  auto& tn = endTimeSync(m_model, scenario);
  m_triggerLine = new TriggerInspectorWidget{
      ctx, ctx.app.interfaces<Command::TriggerCommandFactoryList>(),
      tn, parent};
  widgets.push_back(new TextLabel(QObject::tr("Trigger")));
  widgets.push_back(m_triggerLine);
}

void BaseIntervalInspectorDelegate::addWidgets_post(
    std::vector<QWidget*>& widgets, IntervalInspectorWidget* parent)
{
}

void BaseIntervalInspectorDelegate::on_defaultDurationChanged(
    OngoingCommandDispatcher& dispatcher,
    const TimeVal& val,
    ExpandMode expandmode) const
{
  auto& scenario = *safe_cast<BaseScenario*>(m_model.parent());
  dispatcher.submitCommand<Command::MoveBaseEvent<BaseScenario>>(
      scenario, scenario.endEvent().id(), val, 0., expandmode, LockMode::Free);
}
}
