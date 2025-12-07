#pragma once
#include <Process/ExecutionCommand.hpp>
#include <Process/TimeValue.hpp>

#include <ossia/detail/lockfree_queue.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/scenario/time_value.hpp>

#include <score_lib_process_export.h>

#include <atomic>
#include <memory>
namespace ossia
{
class graph_node;
class graph_interface;
struct execution_state;

#if __cplusplus > 201703L
// TODO moveme
static const constexpr struct disable_init_key_t
{
} disable_init;
struct disable_init_t
{
  disable_init_t(disable_init_key_t) { }
  disable_init_t() = delete;
  disable_init_t(const disable_init_t&) = delete;
  disable_init_t(disable_init_t&&) = delete;
  disable_init_t& operator=(const disable_init_t&) = delete;
  disable_init_t& operator=(disable_init_t&&) = delete;
};
#endif

namespace net
{
class node_base;
}
}
namespace score
{
struct DocumentContext;
template <typename T, typename U, typename V>
class GenericComponentFactoryList;
}
namespace State
{
struct Address;
}
namespace Process
{
class ProcessModel;
}
namespace Execution
{
struct Transaction;
class ProcessComponent;
class ProcessComponentFactory;
class ProcessComponentFactoryList;
struct SetupContext;
namespace Settings
{
class Model;
}

using time_function = smallfun::function<ossia::time_value(const TimeVal&)>;
using reverse_time_function = smallfun::function<TimeVal(const ossia::time_value&)>;

struct ExecutionCommandQueue : private moodycamel::ReaderWriterQueue<ExecutionCommand>
{
public:
  using ReaderWriterQueue<ExecutionCommand>::ReaderWriterQueue;
  template <typename... Args>
  inline auto enqueue(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
    return ReaderWriterQueue<ExecutionCommand>::enqueue(std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline auto enqueue_bulk(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
    return ReaderWriterQueue<ExecutionCommand>::enqueue(std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline auto try_enqueue(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
    return ReaderWriterQueue<ExecutionCommand>::enqueue(std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline auto try_enqueue_bulk(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Ui);
    return ReaderWriterQueue<ExecutionCommand>::enqueue(std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline auto try_dequeue(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD(ossia::thread_type::Audio);
    return ReaderWriterQueue<ExecutionCommand>::try_dequeue(std::forward<Args>(args)...);
  }
};

struct EditionCommandQueue : private moodycamel::ConcurrentQueue<ExecutionCommand>
{
public:
  using ConcurrentQueue<ExecutionCommand>::ConcurrentQueue;
  template <typename... Args>
  inline auto enqueue(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    return ConcurrentQueue<ExecutionCommand>::enqueue(std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline auto enqueue_bulk(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    return ConcurrentQueue<ExecutionCommand>::enqueue(std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline auto try_enqueue(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    return ConcurrentQueue<ExecutionCommand>::enqueue(std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline auto try_enqueue_bulk(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    return ConcurrentQueue<ExecutionCommand>::enqueue(std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline auto try_dequeue(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    return ConcurrentQueue<ExecutionCommand>::try_dequeue(std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline auto try_dequeue_bulk(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    return ConcurrentQueue<ExecutionCommand>::try_dequeue_bulk(
        std::forward<Args>(args)...);
  }
};

struct GCCommandQueue : private moodycamel::ConcurrentQueue<GCCommand>
{
public:
  using ConcurrentQueue<GCCommand>::ConcurrentQueue;
  template <typename... Args>
  inline auto enqueue(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    return ConcurrentQueue<GCCommand>::enqueue(std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline auto enqueue_bulk(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    return ConcurrentQueue<GCCommand>::enqueue(std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline auto try_enqueue(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    return ConcurrentQueue<GCCommand>::enqueue(std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline auto try_enqueue_bulk(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    return ConcurrentQueue<GCCommand>::enqueue(std::forward<Args>(args)...);
  }

  template <typename... Args>
  inline auto try_dequeue(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    return ConcurrentQueue<GCCommand>::try_dequeue(std::forward<Args>(args)...);
  }
  template <typename... Args>
  inline auto try_dequeue_bulk(Args&&... args) -> decltype(auto)
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    return ConcurrentQueue<GCCommand>::try_dequeue_bulk(std::forward<Args>(args)...);
  }
};

//! Useful structures when creating the execution elements.
//!
struct SCORE_LIB_PROCESS_EXPORT Context
{
  std::weak_ptr<void> alias;

  auto acquireExecutionQueue() const noexcept
  {
    return std::shared_ptr<Execution::ExecutionCommandQueue>(
        alias.lock(), &executionQueue);
  }
  auto acquireEditionQueue() const noexcept
  {
    return std::shared_ptr<Execution::EditionCommandQueue>(alias.lock(), &editionQueue);
  }
  auto acquireGCQueue() const noexcept
  {
    return std::shared_ptr<Execution::GCCommandQueue>(alias.lock(), &gcQueue);
  }
  std::weak_ptr<const Context> weakSelf() const noexcept
  {
    return std::shared_ptr<const Context>(alias.lock(), this);
  }
  std::weak_ptr<Execution::ExecutionCommandQueue> weakExecutionQueue() const noexcept
  {
    return std::shared_ptr<Execution::ExecutionCommandQueue>(
        alias.lock(), &executionQueue);
  }
  std::weak_ptr<Execution::EditionCommandQueue> weakEditionQueue() const noexcept
  {
    return std::shared_ptr<Execution::EditionCommandQueue>(alias.lock(), &editionQueue);
  }
  std::weak_ptr<Execution::GCCommandQueue> weakGCQueue() const noexcept
  {
    return std::shared_ptr<Execution::GCCommandQueue>(alias.lock(), &gcQueue);
  }

  const score::DocumentContext& doc;
  const std::atomic_bool& created;

  /** Used to map the "high-level" durations in score to low-level durations
   *
   * For instance, milliseconds to microseconds
   * or milliseconds to samples
   */
  Execution::time_function time;
  Execution::reverse_time_function reverseTime;

  //! \see LiveModification
  ExecutionCommandQueue& executionQueue;
  EditionCommandQueue& editionQueue;
  GCCommandQueue& gcQueue;
  SetupContext& setup;

  const std::shared_ptr<ossia::graph_interface>& execGraph;
  const std::shared_ptr<ossia::execution_state>& execState;

  auto& context() const { return *this; }

#if !defined(_MSC_VER)
#if __cplusplus > 201703L
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-attributes"
  [[no_unique_address]] ossia::disable_init_t disable_copy;
#pragma clang diagnostic pop
#else
  Context() = delete;
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(Context&&) = delete;
#endif
#endif
};

}

#define in_exec this->system().executionQueue.enqueue
#define in_edit this->system().editionQueue.enqueue
#define weak_exec this->system().weakExecutionQueue()
#define weak_edit this->system().weakEditionQueue()
#define weak_gc this->system().weakGCQueue()
