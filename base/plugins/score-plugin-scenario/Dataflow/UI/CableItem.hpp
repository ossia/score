#pragma once
#include <QGraphicsItem>
#include <QObject>
#include <score/model/Identifier.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>

#include <ossia/detail/ptr_set.hpp>
namespace Dataflow {
  class CableItem;
}
extern template class tsl::hopscotch_map<Process::Cable*, Dataflow::CableItem*, ossia::EgurHash<std::remove_pointer_t<Process::Cable*>>>;
namespace Dataflow
{
class PortItem;
class CableItem final
    : public QObject
    , public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

  public:
    static bool g_cables_enabled;
    CableItem(Process::Cable& c, const score::DocumentContext& ctx, QGraphicsItem* parent = nullptr);
    ~CableItem();
    const Id<Process::Cable>& id() const { return m_cable.id(); }
    void resize();
    void check();
    PortItem* source() const;
    PortItem* target() const;
    void setSource(PortItem* p);
    void setTarget(PortItem* p);

    using cable_map = ossia::ptr_map<Process::Cable*, Dataflow::CableItem*>;
    static cable_map& g_cables();

  Q_SIGNALS:
    void clicked();
    void removeRequested();

  private:
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    QPainterPath shape() const override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

    Process::Cable& m_cable;
    QPointer<PortItem> m_p1, m_p2;
    QPainterPath m_path;
    Process::PortType m_type{};
    int8_t a1{}, a2{}, a3{}, a4{};


};
}
