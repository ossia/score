#pragma once
#include <Inspector/InspectorWidgetBase.hpp>

namespace ClipLauncher
{
class LaneModel;
class ProcessModel;

class LaneInspectorWidget final : public Inspector::InspectorWidgetBase
{
public:
  LaneInspectorWidget(
      const LaneModel& lane, const score::DocumentContext& ctx, QWidget* parent);
  ~LaneInspectorWidget();

private:
  const ProcessModel& parentProcess() const;
  const LaneModel& m_lane;
  const score::DocumentContext& m_ctx;
};

} // namespace ClipLauncher
