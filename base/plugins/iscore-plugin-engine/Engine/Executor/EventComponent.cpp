#include <ossia/editor/scenario/time_event.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <QDebug>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <exception>

#include "EventComponent.hpp"
#include <ossia/editor/expression/expression.hpp>
#include <ossia/detail/logger.hpp>

namespace Engine
{
namespace Execution
{
EventComponent::EventComponent(
    const Scenario::EventModel& element,
    const Engine::Execution::Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
    : Execution::Component{ctx, id, "Executor::Event", nullptr}
    , m_iscore_event{element}
{
}

void EventComponent::cleanup()
{
  m_ossia_event.reset();
}

ossia::expression_ptr EventComponent::makeExpression() const
{
  return Engine::iscore_to_ossia::expression(
          m_iscore_event.condition(), system().devices.list());
}

void EventComponent::onSetup(
    std::shared_ptr<ossia::time_event> event,
    ossia::expression_ptr expr,
    ossia::time_event::OffsetBehavior b)
{
  m_ossia_event = event;
  try
  {
    m_ossia_event->setExpression(std::move(expr));
    m_ossia_event->setOffsetBehavior(b);
  }
  catch (std::exception& e)
  {
    ossia::logger().error(e.what());
    m_ossia_event->setExpression(ossia::expressions::make_expression_true());
  }
}

std::shared_ptr<ossia::time_event> EventComponent::OSSIAEvent() const
{
  return m_ossia_event;
}
}
}
