#pragma once
#include <QGraphicsItem>
#include <QObject>
#include <functional>
#if defined(_MSC_VER)
#include <Process/Dataflow/Port.hpp>
#endif
#include <score_plugin_scenario_export.h>
#include <ossia/detail/ptr_set.hpp>
namespace Process { class Port; class Inlet; class Outlet; class ControlInlet; }
namespace score { struct DocumentContext; class Command; }
namespace Scenario { class IntervalModel; }
namespace Dataflow {
  class PortItem;
}
extern template class tsl::hopscotch_map<Process::Port*, Dataflow::PortItem*, ossia::EgurHash<Process::Port*>>;
namespace Dataflow
{
class CableItem;
class SCORE_PLUGIN_SCENARIO_EXPORT PortItem
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
    Process::Port& m_port;
  public:
    PortItem(Process::Port& p, QGraphicsItem* parent);
    ~PortItem() override;
    Process::Port& port() const { return m_port; }
    std::vector<QPointer<CableItem>> cables;

    using port_map = ossia::ptr_map<Process::Port*, Dataflow::PortItem*>;
    static port_map& g_ports();

    static PortItem* clickedPort;

    virtual void setupMenu(QMenu&, const score::DocumentContext& ctx);
    void on_createAutomation(const score::DocumentContext& m_context);
    virtual bool on_createAutomation(
        Scenario::IntervalModel& parent,
        std::function<void(score::Command*)> macro,
        const score::DocumentContext& m_context);

  Q_SIGNALS:
    void showPanel();
    void createCable(PortItem* src, PortItem* snk);
    void contextMenuRequested(QPointF scenepos, QPoint pos);

  private:
    QRectF boundingRect() const final override;
    void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) final override;

    void mousePressEvent(QGraphicsSceneMouseEvent* event) final override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) final override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) final override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) final override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) final override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) final override;

    void dragEnterEvent(QGraphicsSceneDragDropEvent* event) final override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent* event) final override;
    void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) final override;
    void dropEvent(QGraphicsSceneDragDropEvent* event) final override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) final override;


    double m_diam = 6.;
};
SCORE_PLUGIN_SCENARIO_EXPORT
void setupSimpleInlet(PortItem* item, Process::Inlet& port, const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context);
SCORE_PLUGIN_SCENARIO_EXPORT
PortItem* setupInlet(Process::Inlet& port, const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context);
SCORE_PLUGIN_SCENARIO_EXPORT
PortItem* setupOutlet(Process::Outlet& port, const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context);

}

namespace score
{
namespace mime
{
inline const constexpr char* port()
{
  return "application/x-score-port";
}
}
}
