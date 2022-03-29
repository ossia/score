#pragma once
#include <QGraphicsItem>
#include <QObject>

#include <functional>
#include <verdigris>
#if defined(_MSC_VER)
#include <Process/Dataflow/Port.hpp>
#endif
#include <Process/Dataflow/PortType.hpp>

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
class SimpleTextItem;
}
namespace Dataflow
{
class PortItem;
}
namespace Dataflow
{
class CableItem;
struct DragMoveFilter;
class SCORE_LIB_PROCESS_EXPORT PortItem
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(PortItem)
  Q_INTERFACES(QGraphicsItem)
public:
  PortItem(
      const Process::Port& p,
      const Process::Context& ctx,
      QGraphicsItem* parent);
  ~PortItem() override;
  const Process::Port& port() const { return m_port; }

  static PortItem* clickedPort;

  virtual void setupMenu(QMenu&, const score::DocumentContext& ctx);
  void setPortVisible(bool b);
  void resetPortVisible();

  void setHighlight(bool b);

  using QGraphicsItem::dropEvent;
  static const constexpr int Type = QGraphicsItem::UserType + 700;
  int type() const final override { return Type; }

  void createCable(PortItem* src, PortItem* snk)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, createCable, src, snk)
  void contextMenuRequested(QPointF scenepos, QPoint pos)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, contextMenuRequested, scenepos, pos)

protected:
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) final override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) final override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) final override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) final override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) final override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) final override;
  QVariant
  itemChange(GraphicsItemChange change, const QVariant& value) override;

  const Process::Context& m_context;
  std::vector<QPointer<CableItem>> cables;
  const Process::Port& m_port;

public:
  double m_diam = 8.;

protected:
  bool m_visible{true};
  bool m_inlet{true};
  bool m_highlight{true};

  friend class Dataflow::CableItem;

  static const QPixmap&
  portImage(Process::PortType t, bool inlet, bool small, bool light) noexcept;
};

SCORE_LIB_PROCESS_EXPORT
score::SimpleTextItem* makePortLabel(const Process::Port& port, QGraphicsItem* parent);
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
