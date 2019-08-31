#pragma once
#include <QGraphicsItem>
#include <QObject>

#include <verdigris>
#include <functional>
#if defined(_MSC_VER)
#include <Process/Dataflow/Port.hpp>
#endif
#include <ossia/detail/ptr_set.hpp>

#include <score_lib_process_export.h>
namespace Process
{
class Port;
class Inlet;
class Outlet;
class ControlInlet;
struct Context;
}
namespace score
{
struct Brush;
struct DocumentContext;
class Command;
}
namespace Dataflow
{
class PortItem;
}
namespace Dataflow
{
class CableItem;
class SCORE_LIB_PROCESS_EXPORT PortItem : public QObject, public QGraphicsItem
{
  W_OBJECT(PortItem)
  Q_INTERFACES(QGraphicsItem)
public:
  PortItem(
      Process::Port& p,
      const Process::Context& ctx,
      QGraphicsItem* parent);
  ~PortItem() override;
  Process::Port& port() const { return m_port; }

  static PortItem* clickedPort;

  virtual void setupMenu(QMenu&, const score::DocumentContext& ctx);
  void setPortVisible(bool b);
  void resetPortVisible();

public:
  void createCable(PortItem* src, PortItem* snk)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, createCable, src, snk)
  void contextMenuRequested(QPointF scenepos, QPoint pos)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, contextMenuRequested, scenepos, pos)

protected:
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
  QVariant
  itemChange(GraphicsItemChange change, const QVariant& value) final override;

  const Process::Context& m_context;
  std::vector<QPointer<CableItem>> cables;
  Process::Port& m_port;
  double m_diam = 6.;
  bool m_visible{true};
  bool m_inlet{true};

  friend class Dataflow::CableItem;
};

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

W_REGISTER_ARGTYPE(Dataflow::PortItem*)
