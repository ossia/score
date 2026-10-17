#pragma once
#include <Process/LayerView.hpp>
#include <Process/ZoomHelper.hpp>

#include <Patternist/PatternGrid.hpp>
#include <Patternist/PatternModel.hpp>

#include <optional>

namespace score
{
class QGraphicsNoteChooser;
}
namespace Patternist
{
class ProcessModel;
class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  enum class Mode
  {
    //! The steps share the whole layer: in a node, or in the editor.
    Fit,
    //! The steps fall where they play, on the parent's bar grid.
    Timeline
  };

  explicit View(const Patternist::ProcessModel& model, QGraphicsItem* parent);
  ~View() override;

  void setMode(Mode m);
  //! Mute and solo buttons in the lane headers and step numbers above the grid.
  void setAdvanced(bool b);
  void setTimeGrid(const TimeGrid& g);
  void setZoom(ZoomRatio z);

  //! Lane whose header is at `pos`, or -1.
  int headerLaneAt(QPointF pos) const noexcept;

  //! Set a step while painting: emitted for every step the mouse enters.
  void paintStep(int lane, int step, Patternist::Note n)
      W_SIGNAL(paintStep, lane, step, n);
  void paintFinished() W_SIGNAL(paintFinished);
  void noteChanged(int lane, int note) W_SIGNAL(noteChanged, lane, note);
  void noteChangeFinished() W_SIGNAL(noteChangeFinished);
  void muteToggled(int lane) W_SIGNAL(muteToggled, lane);
  void soloToggled(int lane) W_SIGNAL(soloToggled, lane);

private:
  struct Cell
  {
    int lane{-1};
    int step{-1};
  };

  const Pattern* pattern() const noexcept;
  void updateLanes();
  void layoutLanes();
  void invalidateSlots();
  const std::vector<StepSlot>& slots() const;

  double headerWidth() const noexcept;
  double gridTop() const noexcept;
  double laneHeight() const noexcept;
  QRectF laneRect(int lane) const noexcept;
  QRectF muteRect(int lane) const noexcept;
  QRectF soloRect(int lane) const noexcept;
  bool canEdit() const noexcept;
  Cell cellAt(QPointF pos) const noexcept;

  void paint_impl(QPainter*) const override;
  void paintCells(QPainter*, const Pattern&) const;
  void paintHeader(QPainter*, const Pattern&) const;
  void paintStatus(QPainter*) const;

  void heightChanged(qreal) override;
  void widthChanged(qreal) override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  bool sceneEvent(QEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void paintAt(QPointF pos);
  void endPainting();

  const Patternist::ProcessModel& m_model;

  std::vector<score::QGraphicsNoteChooser*> m_lanes;
  Mode m_mode{Mode::Fit};
  bool m_advanced{};
  TimeGrid m_grid;
  ZoomRatio m_zoom{};
  mutable std::vector<StepSlot> m_slots;
  mutable bool m_slotsValid{};

  int m_execPosition{-1};
  int m_execPattern{-1};
  int m_pendingPattern{-1};

  Cell m_hover;
  //! What the press started painting, until the release.
  std::optional<Note> m_painting;
  Cell m_lastPainted;
  bool m_swallowContextMenu{};
};
}
