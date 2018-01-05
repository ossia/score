#pragma once
#include <QGraphicsItem>
#include <QObject>
#include <unordered_map>
#include <score_plugin_scenario_export.h>
namespace Process { class Port; class Inlet; class Outlet; class ControlInlet; }
namespace score { struct DocumentContext; }
namespace Dataflow
{
class CableItem;
class SCORE_PLUGIN_SCENARIO_EXPORT PortItem
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
    Process::Port& m_port;
  public:
    PortItem(Process::Port& p, QGraphicsItem* parent);
    ~PortItem() override;
    Process::Port& port() const { return m_port; }
    std::vector<QPointer<CableItem>> cables;

    using port_map = std::unordered_map<Process::Port*, Dataflow::PortItem*>;
    static port_map g_ports;

    virtual void setupMenu(QMenu&, const score::DocumentContext& ctx);
    virtual void on_createAutomation(const score::DocumentContext& m_context);
  signals:
    void showPanel();
    void createCable(PortItem* src, PortItem* snk);
    void contextMenuRequested(QPointF scenepos, QPoint pos);

  private:
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


    double m_diam = 6.;
};
SCORE_PLUGIN_SCENARIO_EXPORT
void setupSimpleInlet(PortItem* item, Process::Inlet& port, const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context);
SCORE_PLUGIN_SCENARIO_EXPORT
PortItem* setupInlet(Process::Inlet& port, const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context);
SCORE_PLUGIN_SCENARIO_EXPORT
PortItem* setupOutlet(Process::Outlet& port, const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context);

}
