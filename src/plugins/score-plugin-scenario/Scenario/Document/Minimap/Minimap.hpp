#pragma once

#include <Process/TimeValue.hpp>

#include <QGraphicsItem>

#include <score_plugin_scenario_export.h>

#include <verdigris>
namespace Scenario
{
class SCORE_PLUGIN_SCENARIO_EXPORT Minimap final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(Minimap)
  Q_INTERFACES(QGraphicsItem)
public:
  Minimap();
  void setWidth(double);
  double width() const { return m_width; }
  double leftHandle() const { return m_leftHandle; }
  double rightHandle() const { return m_rightHandle; }

  // These do not send notification
  void setMinDistance(double);
  void setLeftHandle(double);
  void setRightHandle(double);
  void setHandles(double l, double r);

  // This one sends visibleRectChanged
  void modifyHandles(double l, double r);

  // Used when reloading because we don't want clamping to apply
  void restoreHandles(double l, double r);

  void setLargeView();
  void zoomIn();
  void zoomOut();
  void zoom(double z);

public:
  void rescale() W_SIGNAL(rescale);
  void visibleRectChanged(double l, double r) W_SIGNAL(visibleRectChanged, l, r);

private:
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

  void mousePressEvent(QGraphicsSceneMouseEvent*) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent*) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent*) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*) final override;

  void hoverEnterEvent(QGraphicsSceneHoverEvent*) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent*) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override;

  QCursor cursorFor(double pos_x) const;

  static const constexpr double m_height{20.};

  double m_leftHandle{};
  double m_rightHandle{};
  double m_width{100.};
  double m_minDist{10.};

  bool m_hidCursor{false};
  bool m_gripLeft{false};
  bool m_gripRight{false};
  bool m_gripMid{false};
  bool m_setCursor{false};
};
}
