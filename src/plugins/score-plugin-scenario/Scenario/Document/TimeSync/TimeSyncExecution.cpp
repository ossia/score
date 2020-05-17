// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeSyncExecution.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/ExecutionContext.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/editor/expression/expression.hpp>
#include <ossia/editor/scenario/time_sync.hpp>
#include <ossia/editor/state/state.hpp>

#include <exception>

namespace Execution
{
TimeSyncComponent::TimeSyncComponent(
    const Scenario::TimeSyncModel& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : Execution::Component{ctx, id, "Executor::TimeSync", nullptr}, m_score_node{&element}
{
  con(element, &Scenario::TimeSyncModel::triggeredByGui, this, &TimeSyncComponent::on_GUITrigger);

  con(element, &Scenario::TimeSyncModel::activeChanged, this, &TimeSyncComponent::updateTrigger);
  con(element, &Scenario::TimeSyncModel::autotriggerChanged, this, [=](bool b) {
    in_exec([ts = m_ossia_node, b] { ts->set_autotrigger(b); });
  });
  con(element, &Scenario::TimeSyncModel::triggerChanged, this, &TimeSyncComponent::updateTrigger);

  con(element,
      &Scenario::TimeSyncModel::musicalSyncChanged,
      this,
      &TimeSyncComponent::updateTriggerTime);
}

void TimeSyncComponent::cleanup()
{
  in_exec([ts = m_ossia_node] { ts->cleanup(); });
  m_ossia_node.reset();
}

ossia::expression_ptr TimeSyncComponent::makeTrigger() const
{
  if (m_score_node)
  {
    if (m_score_node->active())
    {
      try
      {
        return Engine::score_to_ossia::trigger_expression(
            m_score_node->expression(), *system().execState);
      }
      catch (std::exception& e)
      {
        ossia::logger().error(e.what());
      }
    }
  }

  return ossia::expressions::make_expression_true();
}

void TimeSyncComponent::onSetup(std::shared_ptr<ossia::time_sync> ptr, ossia::expression_ptr exp)
{
  m_ossia_node = ptr;
  m_ossia_node->set_expression(std::move(exp));
  if (m_score_node)
  {
    updateTriggerTime();
    m_ossia_node->set_autotrigger(m_score_node->autotrigger());
    m_ossia_node->set_start(m_score_node->isStartPoint());

    using namespace ossia;
    auto startWait
        = [&edit = system().editionQueue, score_node = m_score_node](ossia::time_value = 0_tv) {
            edit.enqueue([score_node] {
              if (score_node)
              {
                auto v = const_cast<Scenario::TimeSyncModel*>(score_node.data());
                v->setWaiting(true);
              }
            });
          };

    auto endWait = [&edit = system().editionQueue, score_node = m_score_node](bool = false) {
      edit.enqueue([score_node] {
        if (score_node)
        {
          auto v = const_cast<Scenario::TimeSyncModel*>(score_node.data());
          v->setWaiting(false);
        }
      });
    };
    // m_ossia_node->entered_evaluation.add_callback([] { });
    m_ossia_node->entered_triggering.add_callback(startWait);
    m_ossia_node->trigger_date_fixed.add_callback(startWait);

    m_ossia_node->left_evaluation.add_callback(endWait);
    m_ossia_node->finished_evaluation.add_callback(endWait);
    // m_ossia_node->triggered.add_callback([] { });
  }
}

std::shared_ptr<ossia::time_sync> TimeSyncComponent::OSSIATimeSync() const
{
  return m_ossia_node;
}

const Scenario::TimeSyncModel& TimeSyncComponent::scoreTimeSync() const
{
  SCORE_ASSERT(m_score_node);
  return *m_score_node;
}

void TimeSyncComponent::updateTrigger()
{
  auto exp_ptr = std::make_shared<ossia::expression_ptr>(this->makeTrigger());
  this->in_exec([e = m_ossia_node, exp_ptr] {
    bool was_observing = e->is_observing_expression();
    if (was_observing)
      e->observe_expression(false);

    e->set_expression(std::move(*exp_ptr));

    if (was_observing)
      e->observe_expression(true);
  });
}

void TimeSyncComponent::updateTriggerTime()
{
  const auto sync = m_score_node->musicalSync();
  this->in_exec(
      [e = m_ossia_node, sync] { e->set_sync_rate(sync, ossia::quarter_duration<double>); });
}

void TimeSyncComponent::on_GUITrigger()
{
  this->in_exec([e = m_ossia_node] { e->start_trigger_request(); });
}
}
