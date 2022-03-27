#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <vector>
#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsEnum
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsEnum)
  Q_INTERFACES(QGraphicsItem)

protected:
  int m_value{};
  int m_clicking{-1};
  QRectF m_rect;
  QRectF m_smallRect;

public:
  std::vector<QString> array;
  int rows{1};
  int columns{4};

  template <std::size_t N>
  QGraphicsEnum(const std::array<const char*, N>& arr, QGraphicsItem* parent)
      : QGraphicsEnum{parent}
  {
    array.reserve(N);
    for (auto str : arr)
      array.push_back(str);
    updateRect();
  }
  QGraphicsEnum(std::vector<QString> arr, QGraphicsItem* parent)
      : QGraphicsEnum{parent}
  {
    array = std::move(arr);
    updateRect();
  }
  QGraphicsEnum(QGraphicsItem* parent);

  void updateRect();
  void setRect(const QRectF& r);
  void setOneLineRect();
  void setValue(int32_t v);
  int value() const;
  QRectF boundingRect() const override;

public:
  void currentIndexChanged(int arg_1)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, currentIndexChanged, arg_1)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
};
}
