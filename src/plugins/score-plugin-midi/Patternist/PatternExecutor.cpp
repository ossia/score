// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PatternExecutor.hpp"

#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

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
  node->pattern = element.patterns()[element.currentPattern()];
  node->current = 0;

  this->node = node;
  m_ossia_process = std::make_shared<pattern_node_process>(node);

  con(element, &Patternist::ProcessModel::channelChanged, this, [this, node](int c) {
    in_exec([node, c = to_midi_channel(c)] { node->set_channel(c); });
  });
  con(element, &Patternist::ProcessModel::currentPatternChanged, this,
      [this, node, &element](int c) {
    in_exec([node, p = element.patterns()[c]] { node->pattern = p; });
  });
  con(element, &Patternist::ProcessModel::patternsChanged, this,
      [this, node, &element]() {
    in_exec(
        [node, p = element.patterns()[element.currentPattern()]] { node->pattern = p; });
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
