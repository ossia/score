#pragma once
#include <QGraphicsItem>
#include <QObject>
namespace Process { class Port; }

namespace Dataflow
{
class CableItem;
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
    std::vector<QPointer<CableItem>> cables;

    using port_map = std::unordered_map<Process::Port*, Dataflow::PortItem*>;
    static port_map g_ports;

  signals:
    void showPanel();
    void createCable(PortItem* src, PortItem* snk);

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

}
