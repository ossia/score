// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Scenario/Commands/MoveBaseEvent.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>
#include <Scenario/Inspector/TimeSync/TriggerInspectorWidget.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <iscore/tools/std/Optional.hpp>

#include <QByteArray>
#include <QDataStream>
#include <QLabel>
#include <QObject>
#include <QtGlobal>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/widgets/TextLabel.hpp>

#include <QString>

#include "BaseConstraintInspectorDelegate.hpp"
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include <iscore/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/PathSerialization.hpp>
class QWidget;

namespace Scenario
{
BaseConstraintInspectorDelegate::BaseConstraintInspectorDelegate(
    const ConstraintModel& cst)
    : ConstraintInspectorDelegate{cst}
{
}

void BaseConstraintInspectorDelegate::updateElements()
{
  auto& scenario = *safe_cast<BaseScenario*>(m_model.parent());
  auto& tn = endTimeSync(m_model, scenario);
  m_triggerLine->updateExpression(tn.expression());
}

void BaseConstraintInspectorDelegate::addWidgets_pre(
    std::vector<QWidget*>& widgets, ConstraintInspectorWidget* parent)
{
  auto& scenario = *safe_cast<BaseScenario*>(m_model.parent());
  auto& ctx = iscore::IDocument::documentContext(scenario);
  auto& tn = endTimeSync(m_model, scenario);
  m_triggerLine = new TriggerInspectorWidget{
      ctx, ctx.app.interfaces<Command::TriggerCommandFactoryList>(),
      tn, parent};
  m_triggerLine->HideRmButton();
  widgets.push_back(new TextLabel(QObject::tr("Trigger")));
  widgets.push_back(m_triggerLine);
}

void BaseConstraintInspectorDelegate::addWidgets_post(
    std::vector<QWidget*>& widgets, ConstraintInspectorWidget* parent)
{
}

void BaseConstraintInspectorDelegate::on_defaultDurationChanged(
    OngoingCommandDispatcher& dispatcher,
    const TimeVal& val,
    ExpandMode expandmode) const
{
  auto& scenario = *safe_cast<BaseScenario*>(m_model.parent());
  dispatcher.submitCommand<Command::MoveBaseEvent<BaseScenario>>(
      scenario, scenario.endEvent().id(), val, 0., expandmode, LockMode::Free);
}
}
