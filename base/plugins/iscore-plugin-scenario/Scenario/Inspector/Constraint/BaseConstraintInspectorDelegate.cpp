#include <Scenario/Commands/MoveBaseEvent.hpp>
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>
#include <Scenario/Inspector/TimeNode/TriggerInspectorWidget.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <boost/optional/optional.hpp>

#include <iscore/application/ApplicationContext.hpp>
#include <QByteArray>
#include <QDataStream>
#include <QtGlobal>
#include <QLabel>
#include <QObject>

#include <QString>

#include "BaseConstraintInspectorDelegate.hpp"
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include <iscore/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/document/DocumentContext.hpp>
class QWidget;

namespace Scenario
{
BaseConstraintInspectorDelegate::BaseConstraintInspectorDelegate(
        const ConstraintModel& cst):
    ConstraintInspectorDelegate{cst}
{
}

void BaseConstraintInspectorDelegate::updateElements()
{
    auto& scenario = *safe_cast<BaseScenario*>(m_model.parent());
    auto& tn = endTimeNode(m_model, scenario);
    m_triggerLine->updateExpression(tn.trigger()->expression());
}

void BaseConstraintInspectorDelegate::addWidgets_pre(
        std::list<QWidget*>& widgets,
        ConstraintInspectorWidget* parent)
{
    auto& scenario = *safe_cast<BaseScenario*>(m_model.parent());
    auto& ctx = iscore::IDocument::documentContext(scenario);
    auto& tn = endTimeNode(m_model, scenario);
    m_triggerLine = new TriggerInspectorWidget{
                    ctx,
                    ctx.app.components.factory<Command::TriggerCommandFactoryList>(),
                    tn,
                    parent};
    m_triggerLine->HideRmButton();
    widgets.push_back(new QLabel(QObject::tr("Trigger")));
    widgets.push_back(m_triggerLine);
}

void BaseConstraintInspectorDelegate::addWidgets_post(
        std::list<QWidget*>& widgets,
        ConstraintInspectorWidget* parent)
{

}

void BaseConstraintInspectorDelegate::on_defaultDurationChanged(
        OngoingCommandDispatcher& dispatcher,
        const TimeValue& val,
        ExpandMode expandmode) const
{
    auto& scenario = *safe_cast<BaseScenario*>(m_model.parent());
    dispatcher.submitCommand<Command::MoveBaseEvent<BaseScenario>>(
                scenario,
                scenario.endEvent().id(),
                val,
                expandmode);
}
}
