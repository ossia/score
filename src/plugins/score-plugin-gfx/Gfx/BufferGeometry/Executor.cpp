#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/BufferGeometryNode.hpp>
#include <Gfx/BufferGeometry/Process.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/port.hpp>

namespace Gfx::BufferGeometry
{
class buffer_geometry_node final : public gfx_exec_node
{
public:
  buffer_geometry_node(GfxExecutionAction& ctx, const Model& proc)
      : gfx_exec_node{ctx}
      , m_process{proc}
  {
    auto node = std::make_unique<score::gfx::BufferGeometryNode>();
    m_bufferGeometryNode = node.get();
    
    // Copy configuration from process model
    node->config = proc.configuration();
    
    id = exec_context->ui->register_node(std::move(node));
  }

  ~buffer_geometry_node()
  {
    if(id >= 0)
      exec_context->ui->unregister_node(id);
  }

  std::string label() const noexcept override { return "Gfx::buffer_geometry_node"; }

private:
  const Model& m_process;
  score::gfx::BufferGeometryNode* m_bufferGeometryNode{};
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::BufferGeometry::Model& element, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{element, ctx, "gfxBufferGeometryExecutorComponent", parent}
{
  auto n = ossia::make_node<buffer_geometry_node>(
      *ctx.execState, ctx.doc.plugin<DocumentPlugin>().exec, element);

  // Set up texture input connections (up to 4 buffers)
  for(std::size_t i = 0; i < 4 && i < element.inlets().size(); ++i)
  {
    if(auto inlet = qobject_cast<Gfx::TextureInlet*>(element.inlets()[i]))
    {
      n->add_texture();
    }
  }

  // Set up control inputs for configuration
  int control_idx = 4; // Start after texture inlets

  // Vertex Count
  if(auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[control_idx++]))
  {
    auto& p = n->add_control();
    p->value = ctrl->value();
    QObject::connect(
        ctrl, &Process::ControlInlet::valueChanged, this,
        [this, n](const ossia::value& v) {
      if(auto val = v.target<int>())
      {
        this->process().setVertexCount(*val);
      }
    });
  }

  // Position attribute controls (5 controls)
  auto setupAttributeControls
      = [&](int& idx, auto locationSetter, auto formatSetter, auto offsetSetter,
            auto strideSetter, auto bufferSetter) {
    // Location
    if(auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[idx++]))
    {
      auto& p = n->add_control();
      p->value = ctrl->value();
      QObject::connect(
          ctrl, &Process::ControlInlet::valueChanged, this,
          [this, locationSetter](const ossia::value& v) {
        if(auto val = v.target<int>())
          (this->process().*locationSetter)(*val);
      });
    }
    // Format
    if(auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[idx++]))
    {
      auto& p = n->add_control();
      p->value = ctrl->value();
      QObject::connect(
          ctrl, &Process::ControlInlet::valueChanged, this,
          [this, formatSetter](const ossia::value& v) {
        if(auto val = v.target<int>())
          (this->process().*formatSetter)(*val);
      });
    }
    // Offset
    if(auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[idx++]))
    {
      auto& p = n->add_control();
      p->value = ctrl->value();
      QObject::connect(
          ctrl, &Process::ControlInlet::valueChanged, this,
          [this, offsetSetter](const ossia::value& v) {
        if(auto val = v.target<int>())
          (this->process().*offsetSetter)(*val);
      });
    }
    // Stride
    if(auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[idx++]))
    {
      auto& p = n->add_control();
      p->value = ctrl->value();
      QObject::connect(
          ctrl, &Process::ControlInlet::valueChanged, this,
          [this, strideSetter](const ossia::value& v) {
        if(auto val = v.target<int>())
          (this->process().*strideSetter)(*val);
      });
    }
    // Buffer
    if(auto ctrl = qobject_cast<Process::ControlInlet*>(element.inlets()[idx++]))
    {
      auto& p = n->add_control();
      p->value = ctrl->value();
      QObject::connect(
          ctrl, &Process::ControlInlet::valueChanged, this,
          [this, bufferSetter](const ossia::value& v) {
        if(auto val = v.target<int>())
          (this->process().*bufferSetter)(*val);
      });
    }
  };

  // Position attribute
  setupAttributeControls(control_idx, &Model::setPositionLocation, &Model::setPositionFormat, 
                        &Model::setPositionOffset, &Model::setPositionStride, &Model::setPositionBuffer);
  
  // Normal attribute  
  setupAttributeControls(control_idx, &Model::setNormalLocation, &Model::setNormalFormat,
                        &Model::setNormalOffset, &Model::setNormalStride, &Model::setNormalBuffer);
  
  // TexCoord0 attribute
  setupAttributeControls(control_idx, &Model::setTexcoord0Location, &Model::setTexcoord0Format,
                        &Model::setTexcoord0Offset, &Model::setTexcoord0Stride, &Model::setTexcoord0Buffer);
  
  // Color attribute
  setupAttributeControls(control_idx, &Model::setColorLocation, &Model::setColorFormat,
                        &Model::setColorOffset, &Model::setColorStride, &Model::setColorBuffer);
  
  // Tangent attribute
  setupAttributeControls(control_idx, &Model::setTangentLocation, &Model::setTangentFormat,
                        &Model::setTangentOffset, &Model::setTangentStride, &Model::setTangentBuffer);

  // Add geometry output port to the node
  n->root_outputs().push_back(new ossia::geometry_outlet);

  this->node = n;
  m_ossia_process = std::make_shared<ossia::node_process>(n);
  /*
  // Connect to configuration changes
  QObject::connect(
      &element, &Model::configurationChanged, this, 
      [n]() {
        static_cast<buffer_geometry_node*>(n.get())->updateConfiguration();
      });*/
}

void ProcessExecutorComponent::cleanup() { }

ProcessExecutorComponent::~ProcessExecutorComponent() { }

}
