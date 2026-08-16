#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QGraphicsItem>
#include <QObject>
#include <QStringList>

#include <score_lib_base_export.h>

#include <array>
#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsCombo final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsCombo)
  SCORE_GRAPHICS_ITEM_TYPE(30)
  friend struct DefaultComboImpl;
  QRectF m_rect{defaultSliderSize};

public:
  QStringList array;

private:
  int m_value{};
  bool m_grab{};
  bool m_editable{};

public:
  template <std::size_t N>
  QGraphicsCombo(const std::array<const char*, N>& arr, QGraphicsItem* parent)
      : QGraphicsCombo{parent}
  {
    array.reserve(N);
    for(auto str : arr)
      array.push_back(str);

    init();
  }

  QGraphicsCombo(QStringList arr, QGraphicsItem* parent)
      : QGraphicsCombo{parent}
  {
    array = std::move(arr);
    init();
  }

  explicit QGraphicsCombo(QGraphicsItem* parent);

  void init();
  void setRect(const QRectF& r);
  void setValue(int v);
  int value() const;

  //! Whether a value that is not in the list may be entered.
  void setEditable(bool b);
  bool editable() const noexcept { return m_editable; }

  //! Show the drop-down at that position in scene coordinates.
  void openEditor(QPointF scenePos);

  bool moving = false;

  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

  //! Text matching none of the entries; only sent when editable().
  void valueEdited(const QString& text) E_SIGNAL(SCORE_LIB_BASE_EXPORT, valueEdited, text)

  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  bool sceneEvent(QEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
