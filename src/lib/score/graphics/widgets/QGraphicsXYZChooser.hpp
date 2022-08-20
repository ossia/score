#pragma once
#include <score/graphics/widgets/Constants.hpp>
#include <score/graphics/widgets/QGraphicsSpinbox.hpp>

#include <ossia-qt/value_metatypes.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsXYZChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsXYZChooser)
  Q_INTERFACES(QGraphicsItem)
  QRectF m_rect{0., 0., 140., 100.};

private:
  ossia::vec3f m_value{}, m_min{}, m_max{};
  float prev_v[3]{};
  bool m_grab{};

public:
  explicit QGraphicsXYZChooser(QGraphicsItem* parent);
  ~QGraphicsXYZChooser();

  void setPoint(const QPointF& r);
  void setValue(ossia::vec3f v);
  void setRange(ossia::vec3f min = {0.f, 0.f, 0.f}, ossia::vec3f max = {1.f, 1.f, 1.f});
  ossia::vec3f value() const;

  bool moving = false;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  ossia::vec3f scaledValue(float x, float y, float z) const noexcept;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};

class SCORE_LIB_BASE_EXPORT QGraphicsXYZSpinboxChooser final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsXYZSpinboxChooser)
  Q_INTERFACES(QGraphicsItem)
  QRectF m_rect{0., 0., 150., 24.};
  QGraphicsSpinbox m_sb[3];

private:
  bool m_grab{};

public:
  explicit QGraphicsXYZSpinboxChooser(QGraphicsItem* parent);
  ~QGraphicsXYZSpinboxChooser();

  void setPoint(const QPointF& r);
  void setValue(ossia::vec3f v);
  void setRange(ossia::vec3f min = {0.f, 0.f, 0.f}, ossia::vec3f max = {1.f, 1.f, 1.f});
  ossia::vec3f value() const noexcept;
  ossia::vec3f getMin() const noexcept;
  ossia::vec3f getMax() const noexcept;

  bool moving = false;

public:
  void sliderMoved() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderMoved)
  void sliderReleased() E_SIGNAL(SCORE_LIB_BASE_EXPORT, sliderReleased)

private:
  ossia::vec3f scaledValue(float x, float y, float z) const noexcept;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
