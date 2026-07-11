#pragma once
#include <Process/LayerView.hpp>

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <score/model/Identifier.hpp>

#include <QVector>

#include <score_plugin_scenario_export.h>
#include <verdigris>

class SequenceHandleItem;
class SequenceRailItem;

namespace Sequence
{

class SCORE_PLUGIN_SCENARIO_EXPORT SequenceView final : public Process::LayerView
{
  W_OBJECT(SequenceView)

public:
  struct HandleData
  {
    Id<Scenario::TimeSyncModel> tsId;
    double x{}; // pixels from left edge
  };

  // Height of the IS rail strip at the top of the layer: handles' state dots
  // sit on it, double-clicking it inserts a new IS.
  static constexpr double RailHeight = 14.;

  explicit SequenceView(QGraphicsItem* parent);
  ~SequenceView() override;

  void setHandles(const QVector<HandleData>& handles);

  void handleDragMoved(Id<Scenario::TimeSyncModel> tsId, double newX, bool ripple)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, handleDragMoved, tsId, newX, ripple)
  void handleDragReleased(Id<Scenario::TimeSyncModel> tsId, double finalX, bool ripple)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, handleDragReleased, tsId, finalX, ripple)
  void handleDragCancelled()
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, handleDragCancelled)
  void railDoubleClicked(double x)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, railDoubleClicked, x)

protected:
  void paint_impl(QPainter*) const override;

private:
  void updateHandleLines();

  QVector<HandleData> m_handles;
  QVector<SequenceHandleItem*> m_handleLines;
  SequenceRailItem* m_rail{};
};

} // namespace Sequence
