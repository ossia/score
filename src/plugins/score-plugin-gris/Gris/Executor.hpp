#pragma once

#include <Gris/Model.hpp>

#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gris
{
class SpatNode;

class Executor final
    : public Execution::ProcessComponent_T<SpatModel, ossia::node_process>
{
  COMPONENT_METADATA("b5c0e91a-3d47-4f8a-9c21-6ae0d4f37b52")
public:
  Executor(SpatModel& proc, const Execution::Context& ctx, QObject* parent);
  ~Executor() override;
};

using ExecutorFactory = Execution::ProcessComponentFactory_T<Executor>;
} // namespace Gris
