#pragma once
#include <iscore_plugin_engine_export.h>
#include <readerwriterqueue.h>
#include <functional>
#include <ossia/editor/scenario/time_value.hpp>
#include <Process/TimeValue.hpp>
namespace iscore
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

using ExecutionCommand = std::function<void()>;
using ExecutionCommandQueue = moodycamel::ReaderWriterQueue<ExecutionCommand>;

//! Useful structures when creating the execution elements.
struct ISCORE_PLUGIN_ENGINE_EXPORT Context
{
  Context() = delete;
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(Context&&) = delete;

  const iscore::DocumentContext& doc;
  Engine::Execution::BaseScenarioElement& scenario;
  const Explorer::DeviceDocumentPlugin& devices;
  const Engine::Execution::ProcessComponentFactoryList& processes;
  const Engine::Execution::StateProcessComponentFactoryList& stateProcesses;

  /** Used to map the "high-level" durations in i-score to low-level durations
   *
   * For instance, milliseconds to microseconds
   * or milliseconds to samples
   */
  std::function<ossia::time_value(TimeVal)> time;

  //! \see LiveModification
  ExecutionCommandQueue& executionQueue;

  auto& context() const { return *this; }
};

}
}
