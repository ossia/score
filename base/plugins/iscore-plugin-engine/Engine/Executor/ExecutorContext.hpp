#pragma once
#include <iscore_plugin_engine_export.h>
#include <readerwriterqueue.h>
#include <functional>
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

using ProcessComponentFactoryList = iscore::
    GenericComponentFactoryList<Process::ProcessModel, Engine::Execution::DocumentPlugin, Engine::Execution::ProcessComponentFactory>;

using StateProcessComponentFactoryList = iscore::
    GenericComponentFactoryList<Process::StateProcess, Engine::Execution::DocumentPlugin, Engine::Execution::StateProcessComponentFactory>;

using ExecutionCommand = std::function<void()>;
using ExecutionCommandQueue = moodycamel::ReaderWriterQueue<ExecutionCommand>;

struct ISCORE_PLUGIN_ENGINE_EXPORT Context
{
  const iscore::DocumentContext& doc;
  const Engine::Execution::DocumentPlugin& sys;
  const Explorer::DeviceDocumentPlugin& devices;
  const Engine::Execution::ProcessComponentFactoryList& processes;
  const Engine::Execution::StateProcessComponentFactoryList& stateProcesses;
  ExecutionCommandQueue& executionQueue;
};
}
}
