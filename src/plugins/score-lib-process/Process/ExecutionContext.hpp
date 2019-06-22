#pragma once
#include <Process/TimeValue.hpp>

#include <ossia/editor/scenario/time_value.hpp>

#include <readerwriterqueue.h>
#include <score_lib_process_export.h>
#include <smallfun.hpp>

#include <functional>
#include <memory>

namespace ossia
{
class graph_node;
class graph_interface;
struct execution_state;
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
class ProcessComponent;
class ProcessComponentFactory;
class ProcessComponentFactoryList;
struct SetupContext;
namespace Settings
{
class Model;
}

using time_function = smallfun::function<ossia::time_value(const TimeVal&)>;
using reverse_time_function
    = smallfun::function<TimeVal(const ossia::time_value&)>;
using ExecutionCommand
    = smallfun::function<void(), 128, std::max(alignof(std::function<void()>), alignof(double))>;
using ExecutionCommandQueue
    = moodycamel::ReaderWriterQueue<ExecutionCommand, 1024>;

//! Useful structures when creating the execution elements.
struct SCORE_LIB_PROCESS_EXPORT Context
{
#if !defined(_MSC_VER)
  Context() = delete;
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(Context&&) = delete;
#endif

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
  ExecutionCommandQueue& editionQueue;
  SetupContext& setup;

  const std::shared_ptr<ossia::graph_interface>& execGraph;
  const std::shared_ptr<ossia::execution_state>& execState;

  auto& context() const { return *this; }
};
}

#define in_exec system().executionQueue.enqueue
#define in_edit system().editionQueue.enqueue
