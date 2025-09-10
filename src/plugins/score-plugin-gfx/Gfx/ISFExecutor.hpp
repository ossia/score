#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>
namespace isf
{
struct descriptor;
}
namespace Gfx
{
struct ProcessedProgram;
class ISFExecutorComponent : public Execution::ProcessComponent
{
public:
  using Execution::ProcessComponent::ProcessComponent;

  void cleanup() override;

  void init(const Gfx::ProcessedProgram& shader, const Execution::Context& ctx);
  void init(
      const QString& shader, const isf::descriptor& desc, const Execution::Context& ctx);
  void on_shaderChanged(const Gfx::ProcessedProgram& shader);
  void on_shaderChanged(const QString& shader, const isf::descriptor& desc);
  std::pair<ossia::inlets, ossia::outlets> setup_node(Execution::Transaction& transact);

  Process::Inlets m_oldInlets;
  Process::Outlets m_oldOutlets;
};
}
