#include <ossia/editor/scenario/time_event.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <QDebug>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <exception>

#include "EventComponent.hpp"
#include <ossia/editor/expression/expression.hpp>

namespace Engine
{
namespace Execution
{
EventComponent::EventComponent(
    std::shared_ptr<ossia::time_event> event,
    const Scenario::EventModel& element,
    const Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
    : Execution::Component{ctx, id, "Executor::Event", nullptr}
    , m_iscore_event{element}
    , m_ossia_event{event}
{
  try
  {
    auto expr = Engine::iscore_to_ossia::expression(
        m_iscore_event.condition(), system().devices.list());
    m_ossia_event->setExpression(std::move(expr));
    m_ossia_event->setOffsetBehavior(
        (ossia::time_event::OffsetBehavior)element.offsetBehavior());
  }
  catch (std::exception& e)
  {
    qDebug() << e.what();
    m_ossia_event->setExpression(ossia::expressions::make_expression_true());
  }
}

std::shared_ptr<ossia::time_event> EventComponent::OSSIAEvent() const
{
  return m_ossia_event;
}
}
}
