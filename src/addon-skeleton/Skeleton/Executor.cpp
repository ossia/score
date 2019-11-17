#include "Executor.hpp"

#include <Process/ExecutionContext.hpp>

#include <ossia/dataflow/port.hpp>

#include <Skeleton/Process.hpp>
namespace Skeleton
{
class node final : public ossia::nonowning_graph_node
{
public:
  node()
  {
  }

  void
  run(const ossia::token_request& tk, ossia::exec_state_facade s) noexcept override
  {
  }

  std::string label() const noexcept override
  {
    return "skeleton";
  }

private:
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Skeleton::Model& element, const Execution::Context& ctx,
    const Id<score::Component>& id, QObject* parent)
    : ProcessComponent_T{element, ctx, id, "SkeletonExecutorComponent", parent}
{
  auto n = std::make_shared<Skeleton::node>();
  this->node = n;
  m_ossia_process = std::make_shared<ossia::node_process>(n);

  /** Don't forget that the node executes in another thread.
   * -> handle live updates with the in_exec function, e.g. :
   *
   * connect(&element.metadata(), &score::ModelMetadata::ColorChanged,
   *         this, [=] (const QColor& c) {
   *
   *   in_exec([c,n=std::dynamic_pointer_cast<Skeleton::node>(this->node)] {
   *     n->set_color(c);
   *   });
   *
   * });
   */
}
}
