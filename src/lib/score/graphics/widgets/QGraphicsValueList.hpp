#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <ossia/network/value/value.hpp>

#include <QGraphicsItem>
#include <QObject>
#include <QStringList>

#include <score_lib_base_export.h>

#include <vector>

#include <verdigris>

namespace score
{
//! One component of a list element: a number in [min, max], or, when choices
//! is not empty, the index of one of them.
struct ValueListColumn
{
  QString name;
  double min{0.};
  double max{127.};
  double init{0.};
  bool integral{true};
  QStringList choices;
};

/**
 * A list of elements edited in place: each element is a row of spinboxes (or
 * combo boxes for columns with choices). "+" appends a copy of the last row,
 * "x" removes a row, the context menu inserts, duplicates, moves and clears,
 * and "Edit as text" takes the whole list as text ("4 7 10", or rows separated
 * by commas: "4 100, 7 80").
 *
 * Lists of single numbers flow in lines of itemsPerLine() cells; lists of
 * several columns show one row per element under the column names.
 *
 * The value is an ossia list: of numbers for one column, of lists otherwise.
 */
class SCORE_LIB_BASE_EXPORT QGraphicsValueList final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsValueList)
  SCORE_GRAPHICS_ITEM_TYPE(300)

public:
  explicit QGraphicsValueList(QGraphicsItem* parent);
  ~QGraphicsValueList();

  void setColumns(std::vector<ValueListColumn> cols);
  const std::vector<ValueListColumn>& columns() const noexcept { return m_columns; }
  void setCountRange(int min, int max);
  void setItemsPerLine(int n);

  void setValue(const ossia::value& v);
  ossia::value value() const;

  QString toText() const;
  //! False, and the list unchanged, when the text does not parse.
  bool setText(const QString& text);

  //! Opens the text editor for the whole list.
  void editAsText();

  bool moving = false;

  QRectF boundingRect() const override;

public:
  //! A cell is being dragged: the value changes continuously.
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  //! The drag ended.
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)
  //! Rows were added, removed, moved, or the text was edited: one edit.
  void edited() E_SIGNAL(SCORE_LIB_BASE_EXPORT, edited)
  void sizeChanged(QSizeF sz) E_SIGNAL(SCORE_LIB_BASE_EXPORT, sizeChanged, sz)

private:
  using Row = std::vector<double>;

  bool scalar() const noexcept { return m_columns.size() == 1; }
  int maxCount() const noexcept;
  Row defaultRow() const;
  Row clampRow(Row r) const;

  void rebuild();
  void updateCells();
  void structuralEdit();

  //! Row under a point in item coordinates, -1 if none.
  int rowAt(QPointF p) const noexcept;
  QRectF cellRect(int row, int col) const noexcept;
  QRectF removeRect(int row) const noexcept;
  QRectF addRect() const noexcept;
  QRectF popRect() const noexcept;
  qreal headerHeight() const noexcept;
  qreal columnWidth(int col) const noexcept;

  void insertRow(int at, Row r);
  void removeRow(int row);
  void moveRow(int row, int dir);

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

  std::vector<ValueListColumn> m_columns{ValueListColumn{}};
  std::vector<Row> m_rows;
  std::vector<std::vector<QGraphicsItem*>> m_cells;
  QRectF m_rect;
  int m_minCount{0};
  int m_maxCount{-1};
  int m_itemsPerLine{4};
  //! The button under a press, until the release: 0 none, 1 add, 2 pop,
  //! 3 + row for a row's remove button.
  int m_pressed{};
};
}
