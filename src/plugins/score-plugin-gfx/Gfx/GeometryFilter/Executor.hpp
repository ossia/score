#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <ossia/dataflow/node_process.hpp>

namespace Gfx::GeometryFilter
{
class Model;
class ProcessExecutorComponent final
    : public Execution::ProcessComponent_T<Gfx::GeometryFilter::Model, ossia::node_process>
{
  COMPONENT_METADATA("601664ee-9307-498d-b3cb-f0d69e03c278")
public:
  ProcessExecutorComponent(
      Model& element, const Execution::Context& ctx, QObject* parent);

  void cleanup() override;

  void on_shaderChanged();
  std::pair<ossia::inlets, ossia::outlets> setup_node(Execution::Transaction& transact);

  Process::Inlets m_oldInlets;
  Process::Outlets m_oldOutlets;
};

using ProcessExecutorComponentFactory
    = Execution::ProcessComponentFactory_T<ProcessExecutorComponent>;
}
