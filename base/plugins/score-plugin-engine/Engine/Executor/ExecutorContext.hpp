#pragma once
#include <score_plugin_engine_export.h>
#include <readerwriterqueue.h>
#include <functional>
#include <ossia/editor/scenario/time_value.hpp>
#include <Process/TimeValue.hpp>
#include <smallfun.hpp>

namespace ossia
{
class graph_node;
}
namespace score
{
struct DocumentContext;
template <typename T, typename U, typename V>
class GenericComponentFactoryList;
}
namespace Explorer
{
class DeviceDocumentPlugin;
}
namespace Process
{
class ProcessModel;
class StateProcess;
}

namespace Engine
{
namespace Execution
{
class DocumentPlugin;
class ProcessComponent;
class ProcessComponentFactory;
class StateProcessComponent;
class StateProcessComponentFactory;
class ProcessComponentFactoryList;
class StateProcessComponentFactoryList;
class BaseScenarioElement;

using ExecutionCommand = smallfun::SmallFun<void(), 128>;
using ExecutionCommandQueue = moodycamel::ReaderWriterQueue<ExecutionCommand, 1024>;

//! Useful structures when creating the execution elements.
struct SCORE_PLUGIN_ENGINE_EXPORT Context
{
  Context() = delete;
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(Context&&) = delete;

  const score::DocumentContext& doc;
  Engine::Execution::BaseScenarioElement& scenario;
  const Explorer::DeviceDocumentPlugin& devices;
  const Engine::Execution::ProcessComponentFactoryList& processes;
  const Engine::Execution::StateProcessComponentFactoryList& stateProcesses;

  /** Used to map the "high-level" durations in score to low-level durations
   *
   * For instance, milliseconds to microseconds
   * or milliseconds to samples
   */
  std::function<ossia::time_value(const TimeVal&)> time;
  std::function<TimeVal(const ossia::time_value&)> reverseTime;

  //! \see LiveModification
  ExecutionCommandQueue& executionQueue;
  ExecutionCommandQueue& editionQueue;
  DocumentPlugin& plugin;

  auto& context() const { return *this; }
};

}
}

#define in_exec system().executionQueue.enqueue
#define in_edit system().editionQueue.enqueue
