#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <ossia-qt/value_metatypes.hpp>

#include <QGraphicsItem>
#include <QObject>
#include <QPainterPath>

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
class SCORE_LIB_BASE_EXPORT QGraphicsPathGeneratorXY final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsPathGeneratorXY)
  SCORE_GRAPHICS_ITEM_TYPE(110)
public:
  template <typename T>
  friend struct GridWidget;

  //! Kept in sync with spat::Path in the PathGenerator process.
  enum Path
  {
    Linear,
    Circle,
    Spiral,
    Lissajous,
    Rose,
    Polygon
  };

  std::vector<ossia::value> tab;

  static constexpr double width() { return 400.; }
  static constexpr double height() { return 400.; }

  halp::xy_type<float> cursorSize{0.04, 0.04};

  int selectedCursor{-1};
  int selectedSource{-1};
  bool isSelected{};

  double min{0.}, max{1.};

  bool m_grab{};
  ossia::value m_value{};
  ossia::value m_execValue{};
  bool m_hasExec{};
  bool moving = false;
  RightClickImpl* impl{};

  QGraphicsPathGeneratorXY(QGraphicsItem* parent);

  void setPoint(const QPointF& r);
  void setValue(ossia::value v);
  ossia::value value() const;
  void setExecutionValue(const ossia::value& v);
  void setExecutionProgress(double v);
  void resetExecution();

  //! Trajectory shape: mirrors the sibling controls of the process, which the
  //! port factory binds to these.
  void setPathMode(int mode);
  void setRadii(float x, float y);
  void setRatioX(int r);
  void setRatioY(int r);
  void setPhase(float p);

  void setRange(const ossia::value& min, const ossia::value& max);
  void setRange(const ossia::domain& dom);

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  bool sceneEvent(QEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

  const std::vector<ossia::value>& sources() const noexcept;
  QPointF pathPoint(const std::vector<ossia::value>& nodes, double u) const noexcept;
  QRectF progressRect() const noexcept;
  void recomputePaths();
  void commitTab();

  std::vector<QPainterPath> m_paths;
  double m_progress{};
  bool m_hasProgress{};

  int m_pathMode{Linear};
  float m_radiusX{0.2f}, m_radiusY{0.2f};
  int m_ratioX{3}, m_ratioY{2};
  float m_phase{};
};
}
