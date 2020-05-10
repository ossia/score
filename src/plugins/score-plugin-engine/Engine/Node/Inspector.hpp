#pragma once
#include <Inspector/InspectorLayout.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

#include <Control/Widgets.hpp>
#include <Engine/Node/Process.hpp>

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
        inlet,
        ctrl.make_widget(ctrl, inlet, doc, self, self),
        doc,
        vlay,
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

}
