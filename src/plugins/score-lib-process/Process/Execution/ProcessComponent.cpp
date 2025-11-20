// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessComponent.hpp"

#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <ossia/detail/thread.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Execution::ProcessComponent)

namespace Execution
{
ProcessComponent::~ProcessComponent() = default;
ProcessComponentFactory::~ProcessComponentFactory() = default;
ProcessComponentFactoryList::~ProcessComponentFactoryList() = default;

void ProcessComponent::lazy_init() { }

ProcessComponent::ProcessComponent(
    Process::ProcessModel& proc, const Context& ctx, const QString& name,
    QObject* parent)
    : Process::GenericProcessComponent<const Context>{proc, ctx, name, parent}
{
}

void ProcessComponent::cleanup()
{
  OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
  node.reset();
  if(m_ossia_process)
  {
    this->system().setup.unregister_node(process(), m_ossia_process->node);
    in_exec([gcq = weak_gc, proc = std::move(m_ossia_process)] {
      OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);

      if(proc->node.use_count() > 0)
        proc->node->clear();

      // SCORE_ASSERT(proc->node.use_count() <= 1);

      if(auto q = gcq.lock())
        q->enqueue(gc(std::move(proc->node)));
      else
        proc->node.reset();
    });
  }
  m_ossia_process.reset();
}
}
