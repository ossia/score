#pragma once
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>

#include <ossia/detail/ptr_set.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <verdigris>
namespace Dataflow
{
class CableItem;
}

namespace Process
{
struct Context;
}
namespace Dataflow
{
class PortItem;
class SCORE_LIB_PROCESS_EXPORT CableItem final : public QObject, public QGraphicsItem
{
  W_OBJECT(CableItem)
  Q_INTERFACES(QGraphicsItem)

public:
  static bool g_cables_enabled;
  CableItem(const Process::Cable& c, const Process::Context& ctx, QGraphicsItem* parent = nullptr);
  ~CableItem() override;
  const Id<Process::Cable>& id() const { return m_cable.id(); }
  const Process::Cable& model() const { return m_cable; }

  static const constexpr int Type = QGraphicsItem::UserType + 9999;
  int type() const final override { return Type; }

  void resize();
  void check();
  PortItem* source() const noexcept;
  PortItem* target() const noexcept;
  void setSource(PortItem* p);
  void setTarget(PortItem* p);

public:
  void removeRequested() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, removeRequested)

private:
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
  QPainterPath shape() const override;
  QPainterPath opaqueArea() const override;
  bool contains(const QPointF& point) const override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  const Process::Cable& m_cable;
  const Process::Context& m_context;
  QPointer<PortItem> m_p1, m_p2;
  QPainterPath m_path;
  Process::PortType m_type{};
};
}
