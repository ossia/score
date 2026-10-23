#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <ossia/network/value/value.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <array>

#include <verdigris>

namespace score
{
/**
 * Where each degree of an octave goes: one column per input degree, one row
 * per output degree from the root (bottom) to the octave above (top). A column
 * has one lit cell, or none when that degree is muted.
 *
 * A click lights a cell, a click on the lit cell mutes the column, a drag
 * moves the lit cell up and down. The context menu resets, inverts and edits
 * the map as text.
 *
 * The value is an ossia list of 12 integers, -1 for a muted degree.
 */
class SCORE_LIB_BASE_EXPORT QGraphicsNoteMatrix final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsNoteMatrix)
  SCORE_GRAPHICS_ITEM_TYPE(310)

public:
  static constexpr int off = -1;
  static constexpr int columns = 12;
  static constexpr int rows = 13;

  explicit QGraphicsNoteMatrix(QGraphicsItem* parent);
  ~QGraphicsNoteMatrix();

  void setValue(const ossia::value& v);
  ossia::value value() const;

  QString toText() const;
  bool setText(const QString& text);

  bool moving = false;

  QRectF boundingRect() const override;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  QRectF cellRect(int col, int row) const noexcept;
  //! Column and row under a point, -1 when outside the grid
  std::pair<int, int> cellAt(QPointF p) const noexcept;
  void set(std::array<int, columns> map);

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

  std::array<int, columns> m_map{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  //! The column a press grabbed, -1 for none
  int m_grabbed{-1};
  //! Whether the drag has moved the lit cell; a press on it alone mutes
  bool m_dragged{};
  bool m_pressedLit{};
};
}
