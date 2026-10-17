#pragma once
#include <Process/LayerView.hpp>
#include <Process/ZoomHelper.hpp>

#include <Patternist/PatternEdit.hpp>
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
  //! Mute and solo buttons, step numbers, the selected lane and its parameter
  //! bars, a step menu on right-click.
  void setAdvanced(bool b);
  void setTimeGrid(const TimeGrid& g);
  void setZoom(ZoomRatio z);
  void setField(StepField f);
  void setSelectedLane(int lane);
  int selectedLane() const noexcept { return m_selectedLane; }

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
  void laneSelected(int lane) W_SIGNAL(laneSelected, lane);
  //! A bar of the parameter lane was dragged to `value`.
  void fieldEdited(int lane, int step, int field, float value)
      W_SIGNAL(fieldEdited, lane, step, field, value);
  void fieldEditFinished() W_SIGNAL(fieldEditFinished);
  void stepMenuRequested(int lane, int step, QPoint screen)
      W_SIGNAL(stepMenuRequested, lane, step, screen);

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
  const std::vector<StepSlot>& slots(int lane) const;
  //! Position in slots(lane) of the step that plays, or -1.
  int playingSlot(int lane) const noexcept;

  double headerWidth() const noexcept;
  double gridTop() const noexcept;
  double gridBottom() const noexcept;
  double laneHeight() const noexcept;
  QRectF laneRect(int lane) const noexcept;
  QRectF muteRect(int lane) const noexcept;
  QRectF soloRect(int lane) const noexcept;
  QRectF fieldRect() const noexcept;
  bool canEdit() const noexcept;
  Cell cellAt(QPointF pos) const noexcept;
  int fieldStepAt(QPointF pos) const noexcept;

  void paint_impl(QPainter*) const override;
  void paintCells(QPainter*, const Pattern&) const;
  void paintHeader(QPainter*, const Pattern&) const;
  void paintField(QPainter*, const Pattern&) const;
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
  bool sceneEventFilter(QGraphicsItem* watched, QEvent* event) override;
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void paintAt(QPointF pos);
  void editFieldAt(QPointF pos);
  void endPainting();

  const Patternist::ProcessModel& m_model;

  std::vector<score::QGraphicsNoteChooser*> m_lanes;
  Mode m_mode{Mode::Fit};
  bool m_advanced{};
  StepField m_field{StepField::Velocity};
  int m_selectedLane{};
  TimeGrid m_grid;
  ZoomRatio m_zoom{};
  mutable std::vector<std::vector<StepSlot>> m_slots;
  mutable bool m_slotsValid{};
  //! Quarters across the width when the lanes line up in a fit layout.
  mutable double m_span{};

  int m_execPosition{-1};
  int m_execPattern{-1};
  int m_pendingPattern{-1};

  Cell m_hover;
  //! What the press started painting, until the release.
  std::optional<Note> m_painting;
  Cell m_lastPainted;
  bool m_editingField{};
  bool m_swallowContextMenu{};
};
}
