#pragma once

#include <score/graphics/GraphicsLayout.hpp>

#include <QFont>

#include <functional>

namespace score
{

//! Marks an item of a GraphicsHBoxLayout as a stretch: it takes whatever
//! width the row has beyond its content, which puts the items after it at
//! the row's right edge.
SCORE_LIB_BASE_EXPORT void setLayoutStretch(QGraphicsItem& item);

//! Opt-in collapsed controls: unlike setVisible(false), these reserve no cell.
//! The item's scale is restored when it becomes part of the layout again.
SCORE_LIB_BASE_EXPORT void setLayoutExcluded(QGraphicsItem& item, bool excluded);
SCORE_LIB_BASE_EXPORT bool isLayoutExcluded(const QGraphicsItem& item) noexcept;
//! The children a layout places: those not excluded, in order.
SCORE_LIB_BASE_EXPORT QList<QGraphicsItem*> layoutItems(const QGraphicsItem& parent);

class SCORE_LIB_BASE_EXPORT GraphicsHBoxLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsHBoxLayout();

  void layout() override;
  void centerContent() override;
};
class SCORE_LIB_BASE_EXPORT GraphicsVBoxLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsVBoxLayout();
  void layout() override;
  void centerContent() override;
};

//! Table row selected by a press on its background or controls
class SCORE_LIB_BASE_EXPORT GraphicsSelectableRow : public GraphicsHBoxLayout
{
public:
  explicit GraphicsSelectableRow(QGraphicsItem* parent);
  ~GraphicsSelectableRow();

  std::function<void()> onActivated;
  void activate();
  void setSelected(bool selected);

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

protected:
  bool sceneEventFilter(QGraphicsItem* watched, QEvent* event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  bool m_selected{};
  bool m_hovered{};
};

//! Titled vbox padded on all sides. Sizes itself, independent of parent margins.
class SCORE_LIB_BASE_EXPORT GraphicsSectionLayout : public GraphicsVBoxLayout
{
public:
  using GraphicsVBoxLayout::GraphicsVBoxLayout;
  ~GraphicsSectionLayout();

  void setTitle(const QString& title);

  void layout() override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

private:
  qreal titleHeight() const;
  //! Follows the skin's font
  void updateTitleFont();
  void setTitleFont(const QFont& base);
  QString m_title;
  QFont m_titleFontBase;
  QFont m_titleFont;
  qreal m_titleAscent{};
};
}
