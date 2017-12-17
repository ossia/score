// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/scenario/time_event.hpp>
#include <Engine/score2OSSIA.hpp>
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
    const Id<score::Component>& id,
    QObject* parent)
  : Execution::Component{ctx, id, "Executor::Event", nullptr}
  , m_score_event{element}
{
  con(element, &Scenario::EventModel::conditionChanged,
      this, [this] (const auto& expr)
  {
    auto exp_ptr = std::make_shared<ossia::expression_ptr>( this->makeExpression() );
    this->system().executionQueue.enqueue(
          [e = m_ossia_event, exp_ptr]
    {
      e->set_expression(std::move(*exp_ptr));
    });
  });
}

void EventComponent::cleanup()
{
  system().executionQueue.enqueue([ev=m_ossia_event] {
    ev->cleanup();
  });
  m_ossia_event.reset();
}

ossia::expression_ptr EventComponent::makeExpression() const
{
  try
  {
    return Engine::score_to_ossia::condition_expression(
          m_score_event.condition(), system().devices.list());
  }
  catch (std::exception& e)
  {
    ossia::logger().error(e.what());
    return ossia::expressions::make_expression_true();
  }
}

void EventComponent::onSetup(
    std::shared_ptr<ossia::time_event> event,
    ossia::expression_ptr expr,
    ossia::time_event::offset_behavior b)
{
  m_ossia_event = event;
  m_ossia_event->set_expression(std::move(expr));
  m_ossia_event->set_offset_behavior(b);
}

std::shared_ptr<ossia::time_event> EventComponent::OSSIAEvent() const
{
  return m_ossia_event;
}
}
}
