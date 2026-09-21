// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PatternExecutor.hpp"

#include <Process/Dataflow/Port.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/network/value/value_conversion.hpp>

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

  // Built here so that the audio thread only has to move it in.
  con(element, &Patternist::ProcessModel::patternsChanged, this,
      [this, node, &element]() {
    in_exec([node, p = element.patterns()]() mutable { node->patterns = std::move(p); });
  });
  con(ctx.doc.execTimer, &QTimer::timeout, this, [&element, node] {
    int c = node->last;
    element.execPosition(c);
  });
}

void Executor::stop()
{
  ProcessComponent::stop();
  this->process().execPosition(-1);
}
Executor::~Executor() { }
}
