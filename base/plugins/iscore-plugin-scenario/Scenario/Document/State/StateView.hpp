#pragma once
#include <QColor>
#include <iscore/tools/GraphicsItem.hpp>
#include <QRect>
#include <QtGlobal>
#include <iscore/model/ColorReference.hpp>

#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <iscore_plugin_scenario_export.h>

class QGraphicsSceneDragDropEvent;
class QGraphicsSceneMouseEvent;
class QMimeData;
class QPainter;

class QWidget;

namespace Scenario
{
class StateMenuOverlay;
class StatePresenter;

class ISCORE_PLUGIN_SCENARIO_EXPORT StateView final : public GraphicsItem
{
  Q_OBJECT

public:
  StateView(StatePresenter& presenter, QQuickPaintedItem* parent = nullptr);
  virtual ~StateView() = default;

  static constexpr int static_type()
  {
    return 1337 + ItemType::State;
  }
  int type() const override
  {
    return static_type();
  }

  const StatePresenter& presenter() const
  {
    return m_presenter;
  }

  QRectF boundingRect() const override;
  QRectF clipRect() const override;

  void paint(
      QPainter* painter) override;

  void setContainMessage(bool);
  void setSelected(bool arg);

  void changeColor(iscore::ColorRef);
  void setStatus(ExecutionStatus);

signals:
  void dropReceived(const QMimeData*);
  void startCreateMode();

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void hoverEnterEvent(QHoverEvent* event) override;
  void hoverLeaveEvent(QHoverEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragLeaveEvent(QDragLeaveEvent* event) override;

  void dropEvent(QDropEvent* event) override;

private:
  void setDilatation(double);
  StatePresenter& m_presenter;
  StateMenuOverlay* m_overlay{};

  bool m_containMessage{false};
  bool m_selected{false};

  iscore::ColorRef m_color;

  ExecutionStatusProperty m_status{};

  static const constexpr qreal m_radiusFull = 6.;
  static const constexpr qreal m_radiusPoint = 3.5;
  qreal m_dilatationFactor = 1;
};
}
