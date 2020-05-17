#pragma once
#include <Automation/Commands/AutomationCommandFactory.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <State/Address.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

#include <score_plugin_automation_export.h>

#include <vector>
struct DataStreamInput;
struct DataStreamOutput;
/** Note : this command is for internal use only, in recording **/

namespace Automation
{
class ProcessModel;

class SCORE_PLUGIN_AUTOMATION_EXPORT InitAutomation final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), InitAutomation, "InitAutomation")
public:
  // Note : the segments shall be sorted from start to end.
  InitAutomation(
      const ProcessModel& obj,
      const State::AddressAccessor& newaddr,
      double newmin,
      double newmax,
      std::vector<Curve::SegmentData>&& segments);
  InitAutomation(
      const ProcessModel& obj,
      State::AddressAccessor&& newaddr,
      double newmin,
      double newmax,
      std::vector<Curve::SegmentData>&& segments);
  InitAutomation(
      const ProcessModel& obj,
      const State::AddressAccessor& newaddr,
      double newmin,
      double newmax);

public:
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<ProcessModel> m_path;
  State::AddressAccessor m_addr;
  double m_newMin{};
  double m_newMax{};
  std::vector<Curve::SegmentData> m_segments;
};
}
