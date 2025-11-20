// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeSyncExecution.hpp"

#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionTransaction.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
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
    const Scenario::TimeSyncModel& element, const Execution::Context& ctx,
    QObject* parent)
    : Execution::Component{ctx, "Executor::TimeSync", nullptr}
    , m_score_node{&element}
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  con(element, &Scenario::TimeSyncModel::triggeredByGui, this,
      &TimeSyncComponent::on_GUITrigger);

  con(element, &Scenario::TimeSyncModel::activeChanged, this,
      &TimeSyncComponent::updateTrigger);
  con(element, &Scenario::TimeSyncModel::autotriggerChanged, this,
      [this](bool b) { in_exec([ts = m_ossia_node, b] { ts->set_autotrigger(b); }); });
  con(element, &Scenario::TimeSyncModel::triggerChanged, this,
      &TimeSyncComponent::updateTrigger);

  // TODO startChanged
  con(element, &Scenario::TimeSyncModel::musicalSyncChanged, this,
      &TimeSyncComponent::updateTriggerTime);
}

void TimeSyncComponent::cleanup(const std::shared_ptr<TimeSyncComponent>& self)
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  in_exec([self, ts = m_ossia_node, gcq_ptr = weak_gc] {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
    ts->cleanup();
    if(auto gcq = gcq_ptr.lock())
      gcq->enqueue(gc(self));
  });
  m_ossia_node.reset();
}

ossia::expression_ptr TimeSyncComponent::makeTrigger() const
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  if(m_score_node)
  {
    if(m_score_node->active())
    {
      try
      {
        return Engine::score_to_ossia::trigger_expression(
            m_score_node->expression(), *system().execState);
      }
      catch(std::exception& e)
      {
        ossia::logger().error(e.what());
      }
    }
  }

  return ossia::expressions::make_expression_true();
}

struct TimeSyncExecutionCallbacks : public ossia::time_sync_callback
{
  explicit TimeSyncExecutionCallbacks(
      std::weak_ptr<EditionCommandQueue> e,
      const QPointer<const Scenario::TimeSyncModel>& p)
      : edit{e}
      , score_node{p}
  {
  }

  void entered_triggering() override
  {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
    if(auto qed = edit.lock())
      qed->enqueue([score_node = this->score_node] {
        OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
        if(score_node)
        {
          auto v = const_cast<Scenario::TimeSyncModel*>(score_node.data());
          v->setWaiting(true);
        }
      });
  }

  void trigger_date_fixed(ossia::time_value) override { entered_triggering(); }

  void left_evaluation() override
  {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
    if(auto qed = edit.lock())
      qed->enqueue([score_node = this->score_node] {
        OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
        if(score_node)
        {
          auto v = const_cast<Scenario::TimeSyncModel*>(score_node.data());
          v->setWaiting(false);
        }
      });
  }

  void finished_evaluation(bool) override
  {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
    left_evaluation();
  }

private:
  std::weak_ptr<EditionCommandQueue> edit;
  QPointer<const Scenario::TimeSyncModel> score_node;
};

void TimeSyncComponent::onSetup(
    std::shared_ptr<ossia::time_sync> ptr, ossia::expression_ptr exp)
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  m_ossia_node = ptr;
  m_ossia_node->set_expression(std::move(exp));
  if(m_score_node)
  {
    updateTriggerTime();
    if(m_score_node->active())
    {
      m_ossia_node->set_autotrigger(m_score_node->autotrigger());
      m_ossia_node->set_start(m_score_node->isStartPoint());
    }

    m_ossia_node->callbacks.callbacks.push_back(
        new TimeSyncExecutionCallbacks{weak_edit, this->m_score_node});
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
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  auto exp_ptr = std::make_shared<ossia::expression_ptr>(this->makeTrigger());

  bool autotrigger = false, start = false;
  if(m_score_node && m_score_node->active())
  {
    autotrigger = m_score_node->autotrigger();
    start = m_score_node->isStartPoint();
  }

  SCORE_ASSERT(m_ossia_node);
  in_exec([e = m_ossia_node, exp_ptr, autotrigger, start] {
    bool was_observing = e->is_observing_expression();
    if(was_observing)
      e->observe_expression(false);

    e->set_expression(std::move(*exp_ptr));
    e->set_autotrigger(autotrigger);
    e->set_start(start);

    if(was_observing)
      e->observe_expression(true);
  });
}

void TimeSyncComponent::updateTriggerTime()
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  ossia::musical_sync quantRate = m_score_node->musicalSync();
  if(quantRate < 0)
  {
    if(auto parent = Scenario::closestParentInterval(m_score_node->parent()))
    {
      auto parent_metrics = Scenario::closestParentWithMusicalMetrics(parent);
      auto parent_quantif = Scenario::closestParentWithQuantification(parent);

      // We only set a quantization if there is some parent that has some tempo information.
      if(parent_quantif.parent && parent_metrics.lastFound
         && parent_metrics.lastFound->hasTimeSignature())
      {
        quantRate = parent_quantif.parent->quantizationRate();
      }

      if(quantRate < 0)
      {
        quantRate = 0.;
      }
    }
  }

  in_exec([e = m_ossia_node, quantRate] {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
    e->set_sync_rate(quantRate);
  });
}

void TimeSyncComponent::on_GUITrigger()
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  in_exec([e = m_ossia_node] {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
    e->start_trigger_request();
  });
}
}
