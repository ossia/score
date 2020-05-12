// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/ExecutionContext.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncRawPtrExecution.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <ossia/detail/logger.hpp>
#include <ossia/editor/expression/expression.hpp>
#include <ossia/editor/scenario/time_sync.hpp>
#include <ossia/editor/state/state.hpp>
#include <score/tools/Bind.hpp>
#include <ossia/dataflow/execution_state.hpp>


#include <exception>

namespace Execution
{
TimeSyncRawPtrComponent::TimeSyncRawPtrComponent(
    const Scenario::TimeSyncModel& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : Execution::Component{ctx, id, "Executor::Event", nullptr}
    , m_score_node{&element}
{
  con(element,
      &Scenario::TimeSyncModel::triggeredByGui,
      this,
      &TimeSyncRawPtrComponent::on_GUITrigger);

  con(element,
      &Scenario::TimeSyncModel::activeChanged,
      this,
      &TimeSyncRawPtrComponent::updateTrigger);
  con(element,
      &Scenario::TimeSyncModel::autotriggerChanged,
      this,
      [=] (bool b) { in_exec([ts = m_ossia_node,b] { ts->set_autotrigger(b); }); });
  con(element,
      &Scenario::TimeSyncModel::triggerChanged,
      this,
      [this](const State::Expression& expr) { this->updateTrigger(); });


  con(element,
      &Scenario::TimeSyncModel::musicalSyncChanged,
      this,
      &TimeSyncRawPtrComponent::updateTriggerTime);
}

void TimeSyncRawPtrComponent::cleanup()
{
  in_exec([ts = m_ossia_node] { ts->cleanup(); });
  m_ossia_node = nullptr;
}

ossia::expression_ptr TimeSyncRawPtrComponent::makeTrigger() const
{
  if (auto element = m_score_node)
  {
    if (element->active())
    {
      try
      {
        return Engine::score_to_ossia::trigger_expression(
            element->expression(), *system().execState);
      }
      catch (std::exception& e)
      {
        ossia::logger().error(e.what());
      }
    }
  }

  return ossia::expressions::make_expression_true();
}

void TimeSyncRawPtrComponent::onSetup(
    ossia::time_sync* ptr,
    ossia::expression_ptr exp)
{
  m_ossia_node = ptr;
  m_ossia_node->set_expression(std::move(exp));
  if (m_score_node)
  {
    updateTriggerTime();
    m_ossia_node->set_autotrigger(m_score_node->autotrigger());
  }
}

ossia::time_sync* TimeSyncRawPtrComponent::OSSIATimeSync() const
{
  return m_ossia_node;
}

const Scenario::TimeSyncModel& TimeSyncRawPtrComponent::scoreTimeSync() const
{
  SCORE_ASSERT(m_score_node);
  return *m_score_node;
}

void TimeSyncRawPtrComponent::updateTrigger()
{
  auto exp_ptr = std::make_shared<ossia::expression_ptr>(this->makeTrigger());
  this->in_exec([e = m_ossia_node, exp_ptr] {
    bool old = e->is_observing_expression();
    if (old)
      e->observe_expression(false);

    e->set_expression(std::move(*exp_ptr));

    if (old)
      e->observe_expression(true);
  });
}

void TimeSyncRawPtrComponent::updateTriggerTime()
{
  const auto sync = m_score_node->musicalSync();
  this->in_exec([e = m_ossia_node, sync] { e->set_sync_rate(sync, ossia::quarter_duration<double>); });
}

void TimeSyncRawPtrComponent::on_GUITrigger()
{
  this->in_exec([e = m_ossia_node] { e->start_trigger_request(); });
}
}
