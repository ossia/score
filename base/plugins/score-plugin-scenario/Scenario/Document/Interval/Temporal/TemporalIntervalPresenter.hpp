#pragma once
#include <Scenario/Document/Interval/IntervalPresenter.hpp>

#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <score_plugin_scenario_export.h>
class QGraphicsItem;
class QObject;
namespace Process
{
class ProcessModel;
class GraphicsShapeItem;
}
namespace Scenario
{
class SlotHandle;
class SlotHeader;
class TemporalIntervalHeader;
class SCORE_PLUGIN_SCENARIO_EXPORT TemporalIntervalPresenter final
    : public IntervalPresenter
{
  Q_OBJECT

public:
  using view_type = TemporalIntervalView;
  const auto& id() const
  {
    return IntervalPresenter::id();
  } // To please boost::const_mem_fun

  TemporalIntervalPresenter(
      const IntervalModel& viewModel,
      const Process::ProcessPresenterContext& ctx,
      bool handles,
      QGraphicsItem* parentobject,
      QObject* parent);

  virtual ~TemporalIntervalPresenter();

  void updateScaling() override;
  void updateHeight();

  int indexOfSlot(const Process::LayerPresenter&);
  void on_zoomRatioChanged(ZoomRatio val) override;

  void changeRackState();
  void selectedSlot(int) const override;
  TemporalIntervalView* view() const;
  TemporalIntervalHeader* header() const;

  void requestSlotMenu(int slot, QPoint pos, QPointF sp) const;
signals:
  void intervalHoverEnter();
  void intervalHoverLeave();

private:
  double rackHeight() const;
  void createSlot(int pos, const Slot& slt);
  void createLayer(int slot, const Process::ProcessModel& proc);
  void updateProcessShape(int slot, const LayerData& data);
  void removeLayer(const Process::ProcessModel& proc);
  void on_slotRemoved(int pos);
  void updateProcessesShape();
  void updatePositions();
  void on_layerModelPutToFront(int slot, const Process::ProcessModel& proc);
  void on_layerModelPutToBack(int slot, const Process::ProcessModel& proc);
  void on_rackChanged();
  void on_processesChanged(const Process::ProcessModel&);
  void on_requestOverlayMenu(QPointF);
  void on_rackVisibleChanged(bool);
  void on_defaultDurationChanged(const TimeVal&);

  struct SlotPresenter
  {
    SlotHeader* header{};
    Process::GraphicsShapeItem* headerDelegate{};
    SlotHandle* handle{};
    std::vector<LayerData> processes;
  };

  std::vector<SlotPresenter> m_slots;
  bool m_handles{true};
};
class PortItem;
class CableItem
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT

public:
  CableItem(Process::Cable& c, QGraphicsItem* parent = nullptr);
  ~CableItem();
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  const auto& id() const { return m_cable.id(); }
  void resize();
  void check();
  PortItem* source() const { return m_p1; }
  PortItem* target() const { return m_p2; }
  void setSource(PortItem* p) { m_p1 = p; check(); }
  void setTarget(PortItem* p) { m_p2 = p; check(); }

  QPainterPath shape() const override;
private:
  Process::Cable& m_cable;
  QPointer<PortItem> m_p1, m_p2;
  QPainterPath m_path;

};
class PortItem final
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
    Process::Port& m_port;
  public:
    PortItem(Process::Port& p, QGraphicsItem* parent);
    ~PortItem();
    Process::Port& port() const { return m_port; }
    QRectF boundingRect() const override;
    void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

    void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
    void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
    void dropEvent(QGraphicsSceneDragDropEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    std::vector<QPointer<CableItem>> cables;
  signals:
    void showPanel();
    void createCable(PortItem* src, PortItem* snk);

  private:
    double m_diam = 6.;
};

}
