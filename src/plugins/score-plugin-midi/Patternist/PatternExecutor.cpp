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
  node->switch_rate = ossia::convert<float>(element.switchQuantification->value());

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
  node->density = ossia::convert<float>(element.density->value());
  node->velocity_scale = ossia::convert<float>(element.velocity->value());
  node->swing = ossia::convert<float>(element.swing->value());
  node->ratchet_scale
      = std::clamp(ossia::convert<float>(element.ratchet->value()), 1.f, 8.f);
  node->pace_offset = std::clamp(ossia::convert<float>(element.pace->value()), -1.f, 1.f);
  node->repeat_time_value = ossia::convert<ossia::vec2f>(element.repeatTime->value());

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
  con(*element.switchQuantification, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, r = double(ossia::convert<float>(v))] { node->switch_rate = r; });
  });
  con(*element.fill, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, f = ossia::convert<bool>(v)] { node->fill = f; });
  });
  con(*element.density, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, d = ossia::convert<float>(v)] { node->density = d; });
  });
  con(*element.velocity, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, d = ossia::convert<float>(v)] { node->velocity_scale = d; });
  });
  con(*element.swing, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, d = ossia::convert<float>(v)] { node->swing = d; });
  });
  con(*element.ratchet, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, r = std::clamp(ossia::convert<float>(v), 1.f, 8.f)] {
      node->ratchet_scale = r;
    });
  });
  con(*element.pace, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, p = std::clamp(ossia::convert<float>(v), -1.f, 1.f)] {
      node->pace_offset = p;
    });
  });
  con(*element.repeatTime, &Process::ControlInlet::valueChanged, this,
      [this, node](const ossia::value& v) {
    in_exec([node, t = ossia::convert<ossia::vec2f>(v)] { node->repeat_time_value = t; });
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
    // Read while the engine writes them: a step drawn one refresh late is all
    // a torn read can cost.
    const auto& pats = node->patterns;
    const int cur = node->current_pattern;
    const int count = (cur >= 0 && cur < int(pats.size()))
                          ? std::min<int>(pats[cur].lanes.size(), pattern_node::max_lanes)
                          : 0;
    std::vector<int64_t> steps(count);
    for(int i = 0; i < count; i++)
      steps[i] = node->lanes[i].last;
    element->setExecSteps(std::move(steps));
    element->execPosition(node->last);
    element->execPattern(node->current_pattern, node->pending_pattern);
  });
}

void Executor::stop()
{
  ProcessComponent::stop();
  this->process().setExecSteps({});
  this->process().execPosition(-1);
  this->process().execPattern(-1, -1);
}
Executor::~Executor() { }
}
