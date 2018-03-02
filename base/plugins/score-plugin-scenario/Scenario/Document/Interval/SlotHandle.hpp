#pragma once
#include <QGraphicsItem>
#include <QPen>
#include <QRect>
#include <QtGlobal>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <score_plugin_scenario_export.h>
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
namespace Process
{
class Port;
}
namespace Scenario
{
class IntervalPresenter;
class TemporalIntervalPresenter;

class SCORE_PLUGIN_SCENARIO_EXPORT SlotHandle final : public QGraphicsItem
{
public:
  SlotHandle(const IntervalPresenter& slotView, int slotIndex, bool isstatic, QGraphicsItem* parent);

  const IntervalPresenter& presenter() const { return m_presenter; }
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

  const IntervalPresenter& m_presenter;
  qreal m_width{};
  int m_slotIndex{};
  bool m_static{};
};
class SlotHeader final : public QGraphicsItem
{
  public:
    SlotHeader(const IntervalPresenter& slotView, int slotIndex, QGraphicsItem* parent);

    const IntervalPresenter& presenter() const { return m_presenter; }
    static constexpr int static_type()
    {
      return QGraphicsItem::UserType + ItemType::SlotHeader;
    }
    int type() const override
    {
      return static_type();
    }

    int slotIndex() const;
    void setSlotIndex(int);
    static constexpr double headerHeight()
    {
      return 32.;
    }
    static constexpr double handleWidth()
    {
      return 16.;
    }
    static constexpr double menuWidth()
    {
      return 16.;
    }

    void setMini(bool);
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
    const IntervalPresenter& m_presenter;
    qreal m_width{};
    double m_menupos{};
    int m_slotIndex{};
    bool m_mini{false};
};
}
