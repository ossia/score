#pragma once
// Avendish (halp) processes as score::gfx graph nodes, without the execution
// engine: oscr::GfxNode holds a reference to its ProcessModel, so the models
// are owned here and must outlive the GfxPipeline that holds the nodes --
// declare the HalpProcesses before the pipeline at every call site.

#include <Gfx/Graph/Node.hpp>

#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <memory>
#include <vector>

namespace score::test::gfx
{
struct HalpProcesses
{
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  int next = 1;

  template <typename T>
  std::unique_ptr<score::gfx::Node> make(const score::DocumentContext& ctx)
  {
    auto model = std::make_unique<oscr::ProcessModel<T>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{next}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));
    return std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<T>{*raw, {}, Gfx::exec_controls{}, next++, ctx}};
  }
};

/// Deliver control values to a node, in the order of its control inlets.
inline void setInputs(score::gfx::Node& n, std::vector<ossia::value> vals)
{
  score::gfx::Message m;
  m.node_id = n.nodeId;
  for(auto& v : vals)
    m.input.push_back(std::move(v));
  n.process(std::move(m));
}
}
