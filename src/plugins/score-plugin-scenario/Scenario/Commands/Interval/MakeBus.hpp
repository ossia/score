#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/Command.hpp>

#include <ossia/detail/algorithms.hpp>

namespace Scenario
{
class IntervalModel;
class ScenarioDocumentModel;
}
namespace Scenario::Command
{
class SetBus final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetBus, "Make an interval a bus")
public:
  SetBus(const ScenarioDocumentModel& src, const IntervalModel& tgt, bool bus)
      : m_doc{src}, m_itv{tgt}, m_old{ossia::contains(src.busIntervals, &tgt)}, m_new{bus}
  {
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    auto& doc = m_doc.find(ctx);
    auto& itv = m_itv.find(ctx);
    if (m_old)
      doc.addBus(&itv);
    else
      doc.removeBus(&itv);
  }
  void redo(const score::DocumentContext& ctx) const override
  {
    auto& doc = m_doc.find(ctx);
    auto& itv = m_itv.find(ctx);
    if (m_new)
      doc.addBus(&itv);
    else
      doc.removeBus(&itv);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override { s << m_doc << m_itv << m_old << m_new; }
  void deserializeImpl(DataStreamOutput& s) override { s >> m_doc >> m_itv >> m_old >> m_new; }

private:
  Path<ScenarioDocumentModel> m_doc;
  Path<IntervalModel> m_itv;

  bool m_old{}, m_new{};
};
}
