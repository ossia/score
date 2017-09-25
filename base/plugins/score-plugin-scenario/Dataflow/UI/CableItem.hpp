#pragma once
#include <QGraphicsItem>
#include <QObject>
#include <score/model/Identifier.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>

namespace Dataflow
{
class PortItem;
class CableItem
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT

  public:
    CableItem(Process::Cable& c, QGraphicsItem* parent = nullptr);
    ~CableItem();
    const Id<Process::Cable>& id() const { return m_cable.id(); }
    void resize();
    void check();
    PortItem* source() const;
    PortItem* target() const;
    void setSource(PortItem* p);
    void setTarget(PortItem* p);

    using cable_map = std::unordered_map<Process::Cable*, Dataflow::CableItem*>;
    static cable_map g_cables;

  signals:
    void clicked();
  private:
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    QPainterPath shape() const override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

    Process::Cable& m_cable;
    QPointer<PortItem> m_p1, m_p2;
    QPainterPath m_path;


};
}
