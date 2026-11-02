#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::GeometryViewer
{
class Model;
class ProcessExecutorComponent final
    : public Execution::ProcessComponent_T<Gfx::GeometryViewer::Model, ossia::node_process>
{
  COMPONENT_METADATA("69c52c3c-8ae1-4936-8b9a-5b95a6b9a553")
public:
  ProcessExecutorComponent(Model& element, const Execution::Context& ctx, QObject* parent);
  void cleanup() override;

private:
  QMetaObject::Connection m_executionUpdate;
};

using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
