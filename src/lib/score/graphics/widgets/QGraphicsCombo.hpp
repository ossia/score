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
  Q_INTERFACES(QGraphicsItem)
  friend struct DefaultComboImpl;
  QRectF m_rect{defaultSliderSize};

public:
  QStringList array;

private:
  int m_value{};
  bool m_grab{};

public:
  template <std::size_t N>
  QGraphicsCombo(const std::array<const char*, N>& arr, QGraphicsItem* parent)
      : QGraphicsCombo{parent}
  {
    array.reserve(N);
    for(auto str : arr)
      array.push_back(str);
  }

  QGraphicsCombo(QStringList arr, QGraphicsItem* parent)
      : QGraphicsCombo{parent}
  {
    array = std::move(arr);
  }

  QGraphicsCombo(QGraphicsItem* parent);

  void setRect(const QRectF& r);
  void setValue(int v);
  int value() const;

  bool moving = false;

  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
