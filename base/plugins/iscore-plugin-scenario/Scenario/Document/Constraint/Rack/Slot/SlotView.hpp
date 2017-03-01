#pragma once
#include <QQuickPaintedItem>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QtGlobal>

#include <iscore_plugin_scenario_export.h>
class QGraphicsSceneContextMenuEvent;
class QPainter;

class QWidget;

namespace Scenario
{
class SlotHandle;
class SlotOverlay;
class SlotPresenter;

class ISCORE_PLUGIN_SCENARIO_EXPORT SlotView final : public QQuickPaintedItem
{
  Q_OBJECT


public:
  const SlotPresenter& presenter;

  SlotView(const SlotPresenter& pres, QQuickPaintedItem* parent);
  virtual ~SlotView() = default;

  void paint(
      QPainter* painter) override;

  void setHeight(qreal height);
  void setWidth(qreal width);

  void enable();
  void disable();

  void setFocus(bool b);

  void setFrontProcessName(const QString&);

signals:
  void askContextMenu(const QPoint&, const QPointF&);

private:
  //void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

  SlotOverlay* m_overlay{};
  SlotHandle* m_handle{};
  bool m_focus{false};
  QString m_frontProcessName{};
};
}
