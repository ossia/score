// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PatternExecutor.hpp"

#include <Process/Dataflow/Port.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QPointer>
#include <QTimer>

#include <Patternist/PatternNode.hpp>

namespace Patternist
{

Executor::Executor(
    Patternist::ProcessModel& element, const Execution::Context& ctx, QObject* parent)
    : ::Execution::ProcessComponent_T<Patternist::ProcessModel, ossia::node_process>{
          element, ctx, "PatternComponent", parent}
{
  auto node = ossia::make_node<pattern_node>(*ctx.execState);
  node->channel = to_midi_channel(element.channel());
  node->in_flight_channel = node->channel;
  node->patterns = element.patterns();
  node->current_pattern = element.currentPattern();
  node->current = 0;
  node->set_switch_rate(ossia::convert<float>(element.switchQuantification->value()));

  this->node = node;
  m_ossia_process = std::make_shared<pattern_node_process>(node);

  element.patternSelect->setupExecution(node->pattern_select, this);
  element.switchQuantification->setupExecution(node->switch_quantification, this);
  element.fill->setupExecution(node->fill_in, this);
  element.density->setupExecution(node->density_in, this);
  element.velocity->setupExecution(node->velocity_in, this);
  element.swing->setupExecution(node->swing_in, this);
  element.ratchet->setupExecution(node->ratchet_in, this);
  element.pace->setupExecution(node->pace_in, this);
  element.repeatTime->setupExecution(node->repeat_time_in, this);
  node->fill = ossia::convert<bool>(element.fill->value());
  node->set_density(ossia::convert<float>(element.density->value()));
  node->set_velocity_scale(ossia::convert<float>(element.velocity->value()));
  node->set_swing(ossia::convert<float>(element.swing->value()));
  node->set_ratchet_scale(ossia::convert<float>(element.ratchet->value()));
  node->set_pace_offset(ossia::convert<float>(element.pace->value()));
  node->set_repeat_time(ossia::convert<ossia::vec2f>(element.repeatTime->value()));

  con(element, &Patternist::ProcessModel::channelChanged, this, [this, node](int c) {
    in_exec([node, c = to_midi_channel(c)] { node->set_channel(c); });
  });

  // The model property is the edit-time selection and the port the automated
  // one: both ask, the node arbitrates and swaps on the next quantum.
  con(element, &Patternist::ProcessModel::currentPatternChanged, this,
      [this, node](int c) { in_exec([node, c] { node->request_pattern(c); }); });
  con(*element.patternSelect, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, c = ossia::convert<int>(v)] { node->request_pattern(c); });
  });
  // Through the node's setters, which hold every value to its range.
  const auto forward = [this, node](Process::ControlInlet& port, auto set) {
    con(port, &Process::ControlInlet::valueChanged, this,
        [this, node, set](const ossia::value& v) {
      in_exec([node, set, v] { set(*node, v); });
    });
  };
  forward(*element.switchQuantification, [](pattern_node& n, const ossia::value& v) {
    n.set_switch_rate(ossia::convert<float>(v));
  });
  forward(*element.fill, [](pattern_node& n, const ossia::value& v) {
    n.fill = ossia::convert<bool>(v);
  });
  forward(*element.density, [](pattern_node& n, const ossia::value& v) {
    n.set_density(ossia::convert<float>(v));
  });
  forward(*element.velocity, [](pattern_node& n, const ossia::value& v) {
    n.set_velocity_scale(ossia::convert<float>(v));
  });
  forward(*element.swing, [](pattern_node& n, const ossia::value& v) {
    n.set_swing(ossia::convert<float>(v));
  });
  forward(*element.ratchet, [](pattern_node& n, const ossia::value& v) {
    n.set_ratchet_scale(ossia::convert<float>(v));
  });
  forward(*element.pace, [](pattern_node& n, const ossia::value& v) {
    n.set_pace_offset(ossia::convert<float>(v));
  });
  forward(*element.repeatTime, [](pattern_node& n, const ossia::value& v) {
    n.set_repeat_time(ossia::convert<ossia::vec2f>(v));
  });

  // Built here so that the audio thread only has to move it in.
  con(element, &Patternist::ProcessModel::patternsChanged, this,
      [this, node, &element]() {
    in_exec([node, p = element.patterns()]() mutable {
      // Swap, not move: the execution queue hands the command to the GC queue
      // once it has run, so the old list is freed on the UI thread instead of
      // this one. Same reason midi_node::replace_notes swaps.
      std::swap(node->patterns, p);
    });
  });
  // The process can be deleted before this component goes away (removing it
  // while playing): the timer must not reach a dead model in between.
  con(ctx.doc.execTimer, &QTimer::timeout, this,
      [element = QPointer{&element}, node] {
    if(!element)
      return;
    const auto& shown = node->shown;
    const int count = std::clamp(
        shown.lane_count.load(std::memory_order_relaxed), 0, pattern_node::max_lanes);
    std::vector<int64_t> steps(count);
    for(int i = 0; i < count; i++)
      steps[i] = shown.lanes[i].load(std::memory_order_relaxed);
    // Only when something moved: every change repaints the layer.
    if(steps != element->execSteps())
    {
      element->setExecSteps(std::move(steps));
      element->execStepsChanged();
    }
    element->execPattern(
        shown.pattern.load(std::memory_order_relaxed),
        shown.pending.load(std::memory_order_relaxed));
  });
}

void Executor::stop()
{
  ProcessComponent::stop();
  this->process().setExecSteps({});
  this->process().execStepsChanged();
  this->process().execPattern(-1, -1);
}
Executor::~Executor() { }
}
