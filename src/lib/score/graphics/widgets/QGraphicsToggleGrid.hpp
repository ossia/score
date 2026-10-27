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
/**
 * Named toggles in rows, one per label. A click toggles a cell and a drag
 * paints the same state over the others; the context menu sets all, none,
 * inverts, and edits the set as text: the labels of the cells that are on,
 * separated by commas.
 *
 * The value is an ossia list of booleans, one per label, in their order.
 */
class SCORE_LIB_BASE_EXPORT QGraphicsToggleGrid
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsToggleGrid)
  SCORE_GRAPHICS_ITEM_TYPE(340)

public:
  explicit QGraphicsToggleGrid(QGraphicsItem* parent);
  ~QGraphicsToggleGrid();

  //! One cell per label, `perRow` cells to a row, all as wide as the widest
  //! label. Every cell starts off.
  void setLabels(QStringList labels, int perRow);
  const QStringList& labels() const noexcept { return m_labels; }

  void setValue(const ossia::value& v);
  ossia::value value() const;

  virtual QString toText() const;
  //! False, and the cells unchanged, when the text names no cell.
  virtual bool setText(const QString& text);

  bool moving = false;

  QRectF boundingRect() const override;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

protected:
  //! Sets every cell and reports one edit.
  void set(std::vector<bool> on);
  const std::vector<bool>& cells() const noexcept { return m_on; }

  virtual QString editTitle() const;
  virtual QString editHint() const;
  virtual QString editError() const;

private:
  QRectF cellRect(int cell) const noexcept;
  int cellAt(QPointF p) const noexcept;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  bool sceneEvent(QEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

  QStringList m_labels;
  std::vector<bool> m_on;
  int m_perRow{8};
  qreal m_cellWidth{16.};
  //! The state a drag paints, set by the press
  bool m_paint{};
  bool m_grab{};
};
}
