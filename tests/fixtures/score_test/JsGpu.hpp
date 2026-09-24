#pragma once

// A GPU Javascript process of a test document with its executor, built outside
// any transport: the test steps the execution ticks and the renders itself.

#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <JS/JSProcessModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/detail/thread.hpp>

#include <QByteArray>

#include <catch2/catch_test_macros.hpp>
#include <score_test/Project.hpp>

#include <memory>
#include <thread>

namespace score::test
{

//! Sets an environment variable for its lifetime.
struct scoped_env
{
  QByteArray key;
  QByteArray old;
  bool existed;

  scoped_env(const char* name, const QByteArray& value)
      : key{name}
      , old{qgetenv(name)}
      , existed{qEnvironmentVariableIsSet(name)}
  {
    qputenv(key.constData(), value);
  }
  scoped_env(const scoped_env&) = delete;
  scoped_env& operator=(const scoped_env&) = delete;
  ~scoped_env()
  {
    if(existed)
      qputenv(key.constData(), old);
    else
      qunsetenv(key.constData());
  }
};

//! Drains \p queue then runs \p tick on a thread pinned as the audio thread.
template <typename Tick>
void run_as_audio_thread(Execution::ExecutionCommandQueue& queue, Tick&& tick)
{
  std::thread worker{[&] {
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    Execution::ExecutionCommand command;
    while(queue.try_dequeue(command))
      command();
    tick();
  }};
  worker.join();
}

//! A Javascript process in the base interval of \p doc running \p qml.
inline JS::ProcessModel&
add_js_process(const score::GUIApplicationContext& ctx, score::Document& doc, const QString& qml)
{
  auto& interval = base_interval(doc);
  const auto key = UuidKey<Process::ProcessModel>::fromString(
      QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0"));
  auto* factory = ctx.interfaces<Process::ProcessFactoryList>().get(key);
  REQUIRE(factory != nullptr);
  CommandDispatcher<>{doc.context().commandStack}
      .submit<Scenario::Command::AddOnlyProcessToInterval>(
          interval, factory->concreteKey(), factory->customConstructionData(), QPointF{});
  JS::ProcessModel* process{};
  for(auto& candidate : interval.processes)
    if(candidate.concreteKey() == key)
      process = qobject_cast<JS::ProcessModel*>(&candidate);
  REQUIRE(process != nullptr);
  const auto program = process->setProgram(JS::QmlSource{qml, {}});
  REQUIRE(program.valid);
  process->programChanged();
  process->inletsChanged();
  process->outletsChanged();
  REQUIRE(process->isGpu());
  return *process;
}

//! The executor of a GPU Javascript process, on an execution context of its
//! own: the document's live audio queue is never drained from a second
//! consumer. Cleans the component up on destruction.
struct js_gpu_executor
{
  std::shared_ptr<Execution::DocumentPlugin::ContextData> context;
  std::shared_ptr<Execution::ProcessComponent> component;
  Gfx::gfx_exec_node* node{};

  js_gpu_executor(
      const score::GUIApplicationContext& ctx, score::Document& doc,
      JS::ProcessModel& process)
      : context{std::make_shared<Execution::DocumentPlugin::ContextData>(doc.context())}
  {
    context->context.alias = context;
    context->execGraph = ossia::make_graph(ossia::graph_setup_options{});
    context->execState = std::make_shared<ossia::execution_state>();
    context->execState->sampleRate = 48000;
    context->execState->bufferSize = 64;
    auto* factory = ctx.interfaces<Execution::ProcessComponentFactoryList>().factory(process);
    REQUIRE(factory != nullptr);
    component = factory->make(process, context->context, nullptr);
    REQUIRE(component != nullptr);
    run_as_audio_thread(context->m_execQueue, [] { });
    // Checked by the caller: a failed REQUIRE here would skip the cleanup.
    node = dynamic_cast<Gfx::gfx_exec_node*>(component->node.get());
  }
  js_gpu_executor(const js_gpu_executor&) = delete;
  js_gpu_executor& operator=(const js_gpu_executor&) = delete;

  ~js_gpu_executor()
  {
    if(component)
    {
      component->cleanup();
      run_as_audio_thread(context->m_execQueue, [] { });
      component.reset();
    }
  }

  //! One execution tick of the node.
  void tick(const ossia::token_request& token)
  {
    run_as_audio_thread(context->m_execQueue, [&] {
      node->run(token, ossia::exec_state_facade{context->execState.get()});
    });
  }
};

}
