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

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

protected:
  score::BrushSet* m_bg{};
  QPixmap* m_pix{};

  qreal m_margin{};
  qreal m_padding{};
  qreal m_spacing{-1.};
};

}
