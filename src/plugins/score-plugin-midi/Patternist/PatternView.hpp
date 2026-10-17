#pragma once
#include <score_plugin_midi_export.h>
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
class SCORE_PLUGIN_MIDI_EXPORT View final : public Process::LayerView
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

  struct Cell
  {
    int lane{-1};
    int step{-1};
  };
  //! Steps selected by dragging in the select mode, or with Ctrl: in every
  //! lane, those under the stretch of time dragged over.
  const StepSelection& selection() const;
  void clearSelection();
  void selectAll();
  void setSelectMode(bool b);
  //! In the editor: the lanes as concentric rings, each its own loop.
  void setRings(bool b);
  bool rings() const noexcept { return m_rings; }

  //! Where copied steps go: the lane under the mouse, or the selection's
  //! first, or the selected lane; and in it and the lanes below, the step at
  //! that time.
  struct PasteTarget
  {
    int lane{-1};
    std::vector<int> starts;
  };
  PasteTarget pasteTarget(int lanes) const;
  StepField field() const noexcept { return m_field; }
  //! The height under which the editor's lanes would get too low to use.
  double minimumHeight() const noexcept;
  //! What a Shift- or Alt-drag in the parameter bars sets, for every step.
  const std::vector<float>& draggedValues() const noexcept { return m_dragValues; }

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
  //! The values of a parameter for every step of a lane changed, while
  //! Shift- or Alt-dragging in the parameter bars: see draggedValues().
  void fieldValuesEdited(int lane, int field) W_SIGNAL(fieldValuesEdited, lane, field);
  void stepMenuRequested(int lane, int step, QPoint screen)
      W_SIGNAL(stepMenuRequested, lane, step, screen);

  void selectionChanged() W_SIGNAL(selectionChanged);
  void fieldSelected(int field) W_SIGNAL(fieldSelected, field);
  //! For `lane`, or every lane when it is -1.
  void fieldRandomized(int lane, int field) W_SIGNAL(fieldRandomized, lane, field);
  void fieldReset(int lane, int field) W_SIGNAL(fieldReset, lane, field);
  //! A lane's steps (0) or rate (1) dragged in its header; 0 follows the
  //! pattern.
  void laneValueEdited(int lane, int which, int value)
      W_SIGNAL(laneValueEdited, lane, which, value);
  void laneValueEditFinished() W_SIGNAL(laneValueEditFinished);

private:
  static constexpr StepField allFields[]
      = {StepField::Velocity, StepField::VelocityRandom, StepField::Chance,
         StepField::Offset,   StepField::Gate,           StepField::Ratchet,
         StepField::Pace,     StepField::Pitch,          StepField::PitchRandom};
  //! Ids of the buttons the editor draws, for the hover.
  static constexpr int mute_button = 1000;
  static constexpr int solo_button = 2000;
  static constexpr int field_button = 3000;
  static constexpr int steps_button = 5000;
  static constexpr int rate_button = 6000;

  QFont nameFont() const;
  const QFont& smallFont() const;
  bool stackedHeader() const noexcept;
  bool laneValues() const noexcept;
  bool ringsMode() const noexcept;
  struct Ring
  {
    double inner{}, outer{};
  };
  QRectF ringArea() const noexcept;
  QPointF ringCenter() const noexcept;
  Ring ring(int lane) const noexcept;
  std::pair<double, double> slotTurns(const StepSlot& sl) const noexcept;
  Cell ringCellAt(QPointF pos) const noexcept;
  void paintRings(QPainter*, const Pattern&) const;
  double cellHeight() const noexcept;
  QRectF valueRect(int lane, int which) const noexcept;
  void paintValue(
      QPainter* painter, QRectF r, const QString& text, bool inherited, int id) const;
  QRectF nameRect(int lane) const noexcept;
  QRectF fieldButtonRect(int i) const noexcept;
  int buttonAt(QPointF pos) const noexcept;
  void pressButton(int id, Qt::KeyboardModifiers mods);
  void paintButton(
      QPainter* painter, QRectF r, const QString& text, bool on, const QColor& onColor,
      int id) const;

  const Pattern* pattern() const noexcept;
  void updateLanes();
  void layoutLanes();
  void invalidateSlots();
  const std::vector<StepSlot>& slots(int lane) const;
  //! Position in slots(lane) of the step that plays, or -1.
  int playingSlot(int lane) const noexcept;

  double headerWidth() const noexcept;
  double measureHeader() const noexcept;
  double fieldButtonHeight() const noexcept;
  bool laneButtons() const noexcept;
  double gridTop() const noexcept;
  double gridBottom() const noexcept;
  double laneHeight() const noexcept;
  QRectF laneRect(int lane) const noexcept;
  QRectF muteRect(int lane) const noexcept;
  QRectF soloRect(int lane) const noexcept;
  double fieldHeight() const noexcept;
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
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  bool sceneEvent(QEvent* event) override;
  bool sceneEventFilter(QGraphicsItem* watched, QEvent* event) override;
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void paintAt(QPointF pos);
  void startFieldDrag(QPointF pos, Qt::KeyboardModifiers mods);
  void dragField(QPointF pos);
  void selectOver(QPointF pos);
  struct SelectionArea
  {
    int lane0{-1}, lane1{-1};
    double x0{}, x1{};
    bool operator==(const SelectionArea&) const noexcept = default;
  };
  void setSelectionArea(SelectionArea a);
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
  mutable std::optional<double> m_headerWidth;
  //! Quarters across the width when the lanes line up in a fit layout.
  mutable double m_span{};

  int m_execPattern{-1};
  int m_pendingPattern{-1};

  Cell m_hover;
  int m_hoverButton{-1};
  struct ValueDrag
  {
    int id{};
    int lane{};
    int which{};
    double y{};
    int start{};
  };
  std::optional<ValueDrag> m_valueDrag;
  double m_hoverX{};
  SelectionArea m_selectionArea;
  mutable StepSelection m_selection;
  mutable bool m_selectionValid{};
  QPointF m_selectAnchor;
  bool m_selecting{};
  bool m_selectMode{};
  bool m_rings{};
  //! What the press started painting, until the release.
  std::optional<Note> m_painting;
  Cell m_lastPainted;
  bool m_editingField{};
  enum class FieldDrag
  {
    None,
    Offset,
    Random
  } m_fieldDrag{FieldDrag::None};
  QPointF m_dragOrigin;
  std::vector<float> m_dragBase;
  std::vector<float> m_dragTarget;
  std::vector<int> m_dragSteps;
  std::vector<float> m_dragValues;
  bool m_swallowContextMenu{};
};
}
