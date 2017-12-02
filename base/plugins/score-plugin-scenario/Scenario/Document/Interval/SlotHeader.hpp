#pragma once

#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <QGraphicsItem>

namespace Scenario
{
class IntervalPresenter;
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
      return 16.;
    }
    static constexpr double handleWidth()
    {
      return 16.;
    }
    static constexpr double menuWidth()
    {
      return 16.;
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
};
}
