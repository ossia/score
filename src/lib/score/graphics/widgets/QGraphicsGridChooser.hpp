#pragma once
#include <score/graphics/widgets/Constants.hpp>
#include <score/graphics/widgets/NoteValues.hpp>

#include <ossia/network/value/vec.hpp>

#include <QGraphicsItem>
#include <QObject>
#include <QString>

#include <score_lib_base_export.h>

#include <array>
#include <verdigris>

class QMenu;
namespace score
{
// A quantization grid: free seconds, a note value, a number of bars or a
// multiple of the process's own period (e.g. its loop). The same knob and
// readout as the time chooser: dragging the knob steps through the values of
// the current unit, clicking the readout opens a menu of the units the
// control allows (and, for note values, of the feels), a right-click on a free
// time types it in, a double-click on the knob goes back to the default.
//
// The value is vec2f{value, unit}, see halp::grid_value: unit 0 is seconds (0
// is no grid, "None"), 1 a fraction of a whole note, 2 bars, 3 a multiple of
// the period, -1 the parent's grid.
class SCORE_LIB_BASE_EXPORT QGraphicsGridChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsGridChooser)
  SCORE_GRAPHICS_ITEM_TYPE(330)
  friend struct DefaultControlImpl;
  friend struct DefaultGraphicsKnobImpl;

public:
  enum Unit : int8_t
  {
    Parent = -1,
    Free = 0,
    Note = 1,
    Bars = 2,
    Period = 3
  };
  //! The bit of a unit in a set of allowed units, as halp::grid_units.
  static constexpr uint32_t flag(Unit u) noexcept
  {
    return u == Parent ? (1u << 4) : (1u << int(u));
  }

  using Feel = note_values::Feel;

  //! The periods the knob steps through, in periods.
  static constexpr std::array<float, 7> periodDetents{0.25f, 0.5f, 1.f, 2.f,
                                                      3.f,   4.f,  8.f};

  //! Free times: 0 to max seconds, the cube of the knob's travel (as the time
  //! chooser), so that short times get most of it.
  double min{0.}, max{8.};
  //! What a double-click goes back to.
  ossia::vec2f init{0.25f, float(Note)};
  bool moving{};

  explicit QGraphicsGridChooser(QGraphicsItem* parent);
  ~QGraphicsGridChooser();

  //! The units the menu offers, or-ed flag()s.
  void setUnits(uint32_t units);
  [[nodiscard]] uint32_t units() const noexcept { return m_units; }
  void setMaxBars(int bars);
  [[nodiscard]] int maxBars() const noexcept { return m_maxBars; }
  //! Lower case, as in "2x loop".
  void setPeriodName(const QString& name);
  [[nodiscard]] const QString& periodName() const noexcept { return m_periodName; }
  void setMaxSeconds(double seconds);

  void setRect(const QRectF& r);
  void setValue(ossia::vec2f v);
  [[nodiscard]] ossia::vec2f value() const noexcept;
  void setExecutionValue(ossia::vec2f v);
  void resetExecution();

  [[nodiscard]] Unit unit() const noexcept { return m_unit; }
  [[nodiscard]] Feel feel() const noexcept { return m_feel; }

  //! Switches to that unit, back to the value it was left at: an edit.
  void chooseUnit(Unit u);
  //! The same note value, dotted or triplet: an edit. Note values only.
  void chooseFeel(Feel f);

  //! What the readout shows.
  [[nodiscard]] QString text() const;
  //! The readout of a value: "None", "250 ms", "1/8T", "3 bars", "2x loop".
  [[nodiscard]] static QString
  label(ossia::vec2f v, const QString& periodName = QStringLiteral("loop"));
  //! A unit, as the menu lists it.
  [[nodiscard]] static QString
  unitName(Unit u, const QString& periodName = QStringLiteral("loop"));

  //! The menu a click on the readout opens, owned by the caller.
  [[nodiscard]] QMenu* makeUnitMenu();

  QRectF boundingRect() const override;

  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  bool sceneEvent(QEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

  //! Free: the seconds of a knob position, and the reverse (what the
  //! right-click type-in box shows and sets).
  double map(double position) const noexcept;
  double unmap(double seconds) const noexcept;
  bool onReadout(QPointF pos) const noexcept;

  //! Knob position, 0..1, of a value in a unit, and the value of a position.
  double positionOf(Unit u, double value) const noexcept;
  double valueAt(Unit u, double position) const noexcept;
  //! A dragged position, on the detents of the current unit.
  double snap(double position, bool allNoteValues) const noexcept;
  void dragTo(double position, bool allNoteValues);
  void popupUnitMenu(QPoint screenPos);
  void edited();

  QRectF m_rect{0., 0., 35., 35.};

  double m_value{}; // knob position, 0..1, in the current unit
  double m_execValue{};
  //! The value of each of free, note, bars and period, as last left: changing
  //! the unit comes back to it.
  std::array<double, 4> m_values{0., 0.25, 1., 1.};
  QString m_periodName{QStringLiteral("loop")};
  uint32_t m_units{flag(Free) | flag(Note) | flag(Bars)};
  int m_maxBars{16};
  Unit m_unit{Note};
  Feel m_feel{Feel::Straight};
  bool m_grab{};
  bool m_hasExec{};
  bool m_hover{};
};
}
