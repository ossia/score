#pragma once
#include <Process/ExecutionCommand.hpp>
#include <Process/TimeValue.hpp>

#include <ossia/detail/lockfree_queue.hpp>
#include <ossia/editor/scenario/time_value.hpp>

#include <score_lib_process_export.h>

#include <atomic>
#include <functional>
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

using ExecutionCommandQueue = ossia::spsc_queue<ExecutionCommand, 1024>;
using EditionCommandQueue = moodycamel::ConcurrentQueue<ExecutionCommand>;
using GCCommandQueue = moodycamel::ConcurrentQueue<GCCommand>;

//! Useful structures when creating the execution elements.
//!
struct SCORE_LIB_PROCESS_EXPORT Context
{
  std::weak_ptr<void> alias;

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

  std::shared_ptr<Execution::Transaction>& transaction;

  void execCommand(ExecutionCommand&& cmd);

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

#define in_exec system().executionQueue.enqueue
#define in_edit system().editionQueue.enqueue
