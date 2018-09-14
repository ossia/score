#pragma once
#include <Inspector/InspectorLayout.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <Engine/Node/Process.hpp>
#include <Engine/Node/Widgets.hpp>

namespace Control
{
template <typename Vis, typename Info>
void visit_ports(Vis&& visitor, const ControlProcess<Info>& proc)
{
  {
    std::size_t i = 0;

    for (auto& port : Info::Metadata::audio_ins)
    {
      visitor(i, port);
      i++;
    }

    for (auto& port : Info::Metadata::midi_ins)
    {
      visitor(i, port);
      i++;
    }

    for (auto& port : Info::Metadata::value_ins)
    {
      visitor(i, port);
      i++;
    }
  }

  {
    std::size_t i = 0;

    for (auto& port : Info::Metadata::audio_outs)
    {
      visitor(i, port);
      i++;
    }

    for (auto& port : Info::Metadata::midi_outs)
    {
      visitor(i, port);
      i++;
    }

    for (auto& port : Info::Metadata::value_outs)
    {
      visitor(i, port);
      i++;
    }
  }
  {
    if constexpr (ossia::safe_nodes::info_functions<Info>::control_count > 0)
    {
      auto start = ossia::safe_nodes::info_functions<Info>::control_start;
      std::size_t i = 0;
      ossia::for_each_in_tuple(
          Info::Metadata::controls, [&](const auto& ctrl) {
            visitor(start + i, ctrl);
            i++;
          });
    }
  }
}

template <typename Proc>
struct inlet_visitor
{
  const Proc& object;
  const score::DocumentContext& doc;
  Inspector::Layout& vlay;
  QWidget* self{};

  void operator()(std::size_t i, const ossia::safe_nodes::audio_in& in)
  {
    auto& inlet = *object.inlets()[i];
    Process::PortWidgetSetup::setupAlone(inlet, doc, vlay, self);
  }
  void operator()(std::size_t i, const ossia::safe_nodes::value_in& in)
  {
    auto& inlet = *object.inlets()[i];
    Process::PortWidgetSetup::setupAlone(inlet, doc, vlay, self);
  }
  void operator()(std::size_t i, const ossia::safe_nodes::midi_in& in)
  {
    auto& inlet = *object.inlets()[i];
    Process::PortWidgetSetup::setupAlone(inlet, doc, vlay, self);
  }
  void operator()(std::size_t i, const ossia::safe_nodes::address_in& in)
  {
    auto& inlet = *object.inlets()[i];
    Process::PortWidgetSetup::setupAlone(inlet, doc, vlay, self);
  }
  template <typename Control_T>
  void operator()(std::size_t i, const Control_T& ctrl)
  {
    auto& inlet = *static_cast<Process::ControlInlet*>(object.inlets()[i]);
    Process::PortWidgetSetup::setupControl(
        inlet, ctrl.make_widget(ctrl, inlet, doc, self, self), doc, vlay,
        self);
  }

  void operator()(std::size_t i, const ossia::safe_nodes::audio_out& out)
  {
    auto& inlet = *object.outlets()[i];
    Process::PortWidgetSetup::setupAlone(inlet, doc, vlay, self);
  }
  void operator()(std::size_t i, const ossia::safe_nodes::value_out& out)
  {
    auto& inlet = *object.outlets()[i];
    Process::PortWidgetSetup::setupAlone(inlet, doc, vlay, self);
  }
  void operator()(std::size_t i, const ossia::safe_nodes::midi_out& out)
  {
    auto& inlet = *object.outlets()[i];
    Process::PortWidgetSetup::setupAlone(inlet, doc, vlay, self);
  }
};

template <typename Info>
class ControlCommand final : public score::Command
{
  using Command::Command;
  ControlCommand() = default;

  ControlCommand(
      const ControlProcess<Info>& obj, int control, ossia::value newval)
      : m_path{obj}
      , m_control{control}
      , m_old{obj.control(control)}
      , m_new{newval}
  {
  }

  virtual ~ControlCommand()
  {
  }

  void undo(const score::DocumentContext& ctx) const final override
  {
    m_path.find(ctx).setControl(m_control, m_old);
  }

  void redo(const score::DocumentContext& ctx) const final override
  {
    m_path.find(ctx).setControl(m_control, m_new);
  }

  void update(unused_t, ossia::value newval)
  {
    m_new = std::move(newval);
  }

protected:
  void serializeImpl(DataStreamInput& stream) const final override
  {
    stream << m_path << m_control << m_old << m_new;
  }
  void deserializeImpl(DataStreamOutput& stream) final override
  {
    stream >> m_path >> m_control >> m_old >> m_new;
  }

private:
  Path<ControlProcess<Info>> m_path;
  int m_control{};
  ossia::value m_old, m_new;
};

template <typename Info>
class InspectorWidget final
    : public Process::InspectorWidgetDelegate_T<ControlProcess<Info>>
{
public:
  explicit InspectorWidget(
      const ControlProcess<Info>& object, const score::DocumentContext& doc,
      QWidget* parent)
      : Process::InspectorWidgetDelegate_T<ControlProcess<Info>>{object,
                                                                 parent}
  {
    using namespace ossia::safe_nodes;
    using meta = typename Info::Metadata;
    auto vlay = new Inspector::Layout{this};

    visit_ports(
        inlet_visitor<ControlProcess<Info>>{object, doc, *vlay, this}, object);
  }

private:
};

template <typename Info>
class InspectorFactory final
    : public Process::InspectorWidgetDelegateFactory_T<
          ControlProcess<Info>, InspectorWidget<Info>>
{
public:
  static Q_DECL_RELAXED_CONSTEXPR
      Process::InspectorWidgetDelegateFactory::ConcreteKey
      static_concreteKey() noexcept
  {
    return Info::Metadata::uuid;
  }

  Process::InspectorWidgetDelegateFactory::ConcreteKey concreteKey() const
      noexcept final override
  {
    return static_concreteKey();
  }
};
}
