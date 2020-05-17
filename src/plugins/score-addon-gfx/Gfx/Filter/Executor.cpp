#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/port.hpp>

#include <Gfx/Filter/Process.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExec.hpp>
#include <Gfx/Graph/filternode.hpp>
#include <Gfx/Graph/isfnode.hpp>
#include <Gfx/TexturePort.hpp>
namespace Gfx::Filter
{
class filter_node final : public gfx_exec_node
{
public:
  filter_node(const QString& frag, GfxExecutionAction& ctx) : gfx_exec_node{ctx}
  {
    auto n = std::make_unique<FilterNode>(frag);

    id = exec_context->ui->register_node(std::move(n));
  }

  filter_node(const isf::descriptor& isf, const QString& frag, GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
    auto n = std::make_unique<ISFNode>(isf, frag);

    id = exec_context->ui->register_node(std::move(n));
  }

  ~filter_node() { exec_context->ui->unregister_node(id); }

  std::string label() const noexcept override { return "Gfx::filter_node"; }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::Filter::Model& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{element, ctx, id, "gfxExecutorComponent", parent}
{
  try
  {

    const auto& desc = element.isfDescriptor();

    auto n = desc.inputs.empty()
                 ? std::make_shared<filter_node>(
                     element.processedFragment(), ctx.doc.plugin<DocumentPlugin>().exec)
                 : std::make_shared<filter_node>(
                     desc, element.processedFragment(), ctx.doc.plugin<DocumentPlugin>().exec);

    int i = 0;
    std::weak_ptr<gfx_exec_node> weak_node = n;
    for (auto& ctl : element.inlets())
    {
      if (auto ctrl = dynamic_cast<Process::ControlInlet*>(ctl))
      {
        auto& p = n->add_control();
        ctl->setupExecution(*n->root_inputs().back());
        *p.value = ctrl->value(); // TODO does this make sense ?
        p.changed = true;         // we will send the first value

        QObject::connect(
            ctrl, &Process::ControlInlet::valueChanged, this, con_unvalidated{ctx, i, weak_node});
        i++;
      }
      else if (auto ctrl = dynamic_cast<Process::AudioInlet*>(ctl))
      {
        n->add_audio();
      }
      else if (auto ctrl = dynamic_cast<Gfx::TextureInlet*>(ctl))
      {
        n->add_texture();
      }
    }
    n->root_outputs().push_back(new ossia::value_outlet);

    this->node = n;
    m_ossia_process = std::make_shared<ossia::node_process>(n);
  }
  catch (...)
  {
  }
}
}
