#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <ossia-qt/value_metatypes.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

#include <halp/value_types.hpp>
#include <halp/custom_widgets.hpp>

namespace ossia
{

struct domain;
}
namespace score
{
template <typename T>
struct GridWidget;
struct RightClickImpl;
class SCORE_LIB_BASE_EXPORT QGraphicsMultiSliderXY final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsMultiSliderXY)
  SCORE_GRAPHICS_ITEM_TYPE(110)
public:
  template <typename T>
  friend struct GridWidget;

  std::vector<ossia::value> tab;

  static constexpr double width() { return 400.; }
  static constexpr double height() { return 400.; }

  halp::xy_type<float> cursorSize{0.04, 0.04};

  int selectedCursor;
  bool isSelected;

  double min{0.}, max{1.};

  int m_grab{-1};
  ossia::value m_value{};
  ossia::value m_execValue{};
  bool m_hasExec{};
  bool moving = false;
  RightClickImpl* impl{};

  QGraphicsMultiSliderXY(QGraphicsItem* parent);

  void setPoint(const QPointF& r);
  void setValue(ossia::value v);
  ossia::value value() const;
  //void setExecutionValue(double v);
  //void resetExecution();

  void setRange(const ossia::value& min, const ossia::value& max);
  void setRange(const ossia::domain& dom);

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
