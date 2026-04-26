#pragma once
#include <Process/LayerView.hpp>

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <score/model/Identifier.hpp>

#include <QVector>

#include <score_plugin_scenario_export.h>
#include <verdigris>

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

  explicit SequenceView(QGraphicsItem* parent);
  ~SequenceView() override;

  void setHandles(const QVector<HandleData>& handles);

  void handleDragMoved(Id<Scenario::TimeSyncModel> tsId, double newX)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, handleDragMoved, tsId, newX)
  void handleDragReleased(Id<Scenario::TimeSyncModel> tsId, double finalX)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, handleDragReleased, tsId, finalX)
  void handleDragCancelled()
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, handleDragCancelled)

protected:
  void paint_impl(QPainter*) const override;
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent*) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override;
  void keyPressEvent(QKeyEvent*) override;

private:
  int handleAt(double x) const; // returns handle index or -1

  QVector<HandleData> m_handles;
  int m_activeHandle{-1};
  double m_dragStartX{};
};

} // namespace Sequence
