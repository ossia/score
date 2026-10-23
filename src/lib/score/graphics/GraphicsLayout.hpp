#pragma once
#include <score/graphics/RectItem.hpp>

namespace score
{
struct BrushSet;
class SCORE_LIB_BASE_EXPORT GraphicsLayout : public score::BackgroundItem
{
public:
  explicit GraphicsLayout(QGraphicsItem* parent);
  ~GraphicsLayout();

  virtual void layout();
  virtual void centerContent();

  void setBrush(score::BrushSet& b);
  void setBackground(const QString& b);
  //! Whether the layout paints a box of its own
  bool hasBackground() const noexcept { return m_bg || m_pix; }
  void setMargin(qreal m);
  void setPadding(qreal p);

  //! Gap between box layout children. Default: 2 * padding
  void setSpacing(qreal s);
  qreal spacing() const noexcept;

  void updateChildrenRects(const QList<QGraphicsItem*>&);

  //! The layout keeps its children's bounding rect plus these margins as its
  //! size, also when relayoutAround() lays it out again.
  void setFitMargins(qreal right, qreal bottom);

  //! Lays out again the containers and layouts around an item whose size
  //! changed, innermost first, and reports it to the outermost layout's owner
  //! through its childrenSizeChanged().
  static void relayoutAround(QGraphicsItem* item);

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

protected:
  score::BrushSet* m_bg{};
  QPixmap* m_pix{};

  qreal m_margin{};
  qreal m_padding{};
  qreal m_spacing{-1.};
  qreal m_fitRight{-1.};
  qreal m_fitBottom{-1.};
};

}
