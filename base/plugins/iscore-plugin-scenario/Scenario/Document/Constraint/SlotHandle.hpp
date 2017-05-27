#pragma once
#include <QGraphicsItem>
#include <QPen>
#include <QRect>
#include <QtGlobal>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <iscore_plugin_scenario_export.h>
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class ConstraintPresenter;

class ISCORE_PLUGIN_SCENARIO_EXPORT SlotHandle final : public QGraphicsItem
{
public:
  SlotHandle(const ConstraintPresenter& slotView, int slotIndex, QGraphicsItem* parent);

  const ConstraintPresenter& presenter() const { return m_presenter; }
  static constexpr int static_type()
  {
    return QGraphicsItem::UserType + ItemType::SlotHandle;
  }
  int type() const override
  {
    return static_type();
  }

  int slotIndex() const;
  void setSlotIndex(int);
  static constexpr double handleHeight()
  {
    return 4.;
  }

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void setWidth(qreal width);

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;

  const ConstraintPresenter& m_presenter;
  qreal m_width{};
  int m_slotIndex;
};

class ISCORE_PLUGIN_SCENARIO_EXPORT InactiveSlotHandle final : public QGraphicsItem
{
public:
  InactiveSlotHandle(const ConstraintPresenter& slotView, int slotIndex, QGraphicsItem* parent);

  int slotIndex() const;
  void setSlotIndex(int);
  static constexpr double handleHeight()
  {
    return 4.;
  }

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void setWidth(qreal width);

private:
  qreal m_width{};
  int m_slotIndex;
};
}
