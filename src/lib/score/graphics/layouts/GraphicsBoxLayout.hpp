#pragma once

#include <score/graphics/GraphicsLayout.hpp>

#include <QFont>

#include <functional>

namespace score
{

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
