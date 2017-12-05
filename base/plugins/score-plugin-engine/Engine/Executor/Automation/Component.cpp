// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/editor/automation/automation.hpp>
#include <ossia/editor/automation/curve_value_visitor.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QDebug>
#include <QString>
#include <algorithm>
#include <vector>

#include "Component.hpp"
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <ossia/editor/curve/curve.hpp>
#include <ossia/editor/curve/curve_segment.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/device.hpp>
#include <Engine/CurveConversion.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/score2OSSIA.hpp>
#include <State/Address.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>

#include <ossia/network/dataspace/dataspace_visitors.hpp> // temporary
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <ossia/dataflow/node_process.hpp>
namespace Automation
{
namespace RecreateOnPlay
{
// concept automation-like: control points, reset(), segments
// TODO put this in ossia::value enum.
struct modvalue
{
  public:
    modvalue(): modvalue(ossia::value{}) {}
    explicit modvalue(ossia::value v):
      m_offset{std::make_unique<ossia::value>()}
    {
      set_offset(std::move(v));
    }
    modvalue(const modvalue& other): modvalue(other.offset()) { }
    modvalue(modvalue&& other): modvalue(std::move(other.offset())) { }

    modvalue& operator=(const modvalue& other)
    {
      set_offset(other.offset());
      return *this;
    }

    modvalue& operator=(modvalue&& other)
    {
      set_offset(std::move(other.offset()));
      return *this;
    }

    const ossia::value& offset() const { return *m_offset; }
    ossia::value& offset() { return *m_offset; }
    void set_offset(const ossia::value& v)
    {
      // if(v.target<delta>()) ... unpack it and store the inner value
      *m_offset = v;
    }
    void set_offset(ossia::value&& v) { *m_offset = std::move(v); }
  private:
    std::unique_ptr<ossia::value> m_offset;
};

// likewise, can be used to set a value between some boundaries.
// the "easy" node handlers should be able to leverage this to produce
// correct values.
struct range_position
{
    float position{0.5};
};

class automation_node final :
    public ossia::graph_node
{
  public:
    automation_node()
    {
      m_outlets.push_back(ossia::make_outlet<ossia::value_port>());
    }

    ~automation_node() override
    {

    }

    void set_destination(optional<ossia::destination> d)
    {
      if(d)
      {
        auto& port = *m_outlets.front();
        port.address = &d->address();
        auto& vp = *port.data.target<ossia::value_port>();
        vp.type = d->unit;
        vp.index = d->index;
      }
      else
      {
        m_outlets.front()->address = {};
      }
    }

    void set_behavior(const ossia::behavior& b)
    {
      m_drive = b;
    }

    void reset_drive()
    {
      m_drive.reset();
    }
  private:
    void run(ossia::token_request t, ossia::execution_state& e) override
    {
      if(!m_drive)
        return;

      auto& outlet = *m_outlets[0];
      ossia::value_port* vp = outlet.data.target<ossia::value_port>();
      ossia::val_type type = ossia::underlying_type(vp->type);
      if(type == ossia::val_type::IMPULSE)
        type = ossia::val_type::FLOAT;

      vp->add_value(
            ossia::apply(
              ossia::detail::compute_value_visitor{t.position, type},
              m_drive), t.date);
    }

    ossia::behavior m_drive;
};

class automation_process : public ossia::node_process
{
public:
    using ossia::node_process::node_process;
  void start() override
  {
      static_cast<automation_node*>(node.get())->reset_drive();
  }
};
Component::Component(
    ::Automation::ProcessModel& element,
    const ::Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ProcessComponent_T{
        element,
        ctx,
        id, "Executor::AutomationComponent", parent}
{
  auto node = std::make_shared<automation_node>();
  auto proc = std::make_shared<automation_process>(node);
  m_ossia_process = proc;
  m_node = node;

  con(element, &Automation::ProcessModel::addressChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Automation::ProcessModel::minChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Automation::ProcessModel::maxChanged,
      this, [this] (const auto&) { this->recompute(); });

  // TODO the tween case will reset the "running" value,
  // so it may not work perfectly.
  con(element, &Automation::ProcessModel::tweenChanged,
      this, [this] (const auto&) { this->recompute(); });
  con(element, &Automation::ProcessModel::curveChanged,
      this, [this] () { this->recompute(); });

  ctx.plugin.register_node(process(), node);
  recompute();
}

Component::~Component()
{
  system().plugin.unregister_node(process(), m_node);
}

void Component::recompute()
{
  auto dest = Engine::score_to_ossia::makeDestination(
        system().devices.list(),
        process().address());

  if (dest)
  {
    auto& d = *dest;
    auto addressType = d.address().get_value_type();

    auto curve = process().tween()
        ? on_curveChanged(addressType, d)
        : on_curveChanged(addressType, {});

    if (curve)
    {
      system().executionQueue.enqueue(
            [proc=std::dynamic_pointer_cast<automation_node>(m_node)
            ,curve
            ,d_=d]
      {
        proc->set_destination(std::move(d_));
        proc->set_behavior(curve);
      });
      return;
    }
  }
  else
  {
    auto curve = on_curveChanged_impl<float>({});

    if (curve)
    {
      system().executionQueue.enqueue(
            [proc=std::dynamic_pointer_cast<automation_node>(m_node)
            ,curve]
      {
        proc->set_destination({});
        proc->set_behavior(curve);
      });
      return;
    }
  }
}

template <typename Y_T>
std::shared_ptr<ossia::curve_abstract>
Component::on_curveChanged_impl(const optional<ossia::destination>& d)
{
  using namespace ossia;

  const double min = process().min();
  const double max = process().max();

  auto scale_x = [](double val) -> double { return val; };
  auto scale_y = [=](double val) -> Y_T { return val * (max - min) + min; };

  auto segt_data = process().curve().sortedSegments();
  if (segt_data.size() != 0)
  {
    return Engine::score_to_ossia::curve<double, Y_T>(
          scale_x, scale_y, segt_data, d);
  }
  else
  {
    return {};
  }
}

std::shared_ptr<ossia::curve_abstract>
Component::on_curveChanged(
    ossia::val_type type,
    const optional<ossia::destination>& d)
{
  switch (type)
  {
    case ossia::val_type::INT:
      return on_curveChanged_impl<int>(d);
    case ossia::val_type::FLOAT:
      return on_curveChanged_impl<float>(d);
    case ossia::val_type::LIST:
    case ossia::val_type::VEC2F:
    case ossia::val_type::VEC3F:
    case ossia::val_type::VEC4F:
      return on_curveChanged_impl<float>(d);
    default:
      qDebug() << "Unsupported curve type: " << (int)type;
      SCORE_TODO;
  }

  return {};
}
}
}
