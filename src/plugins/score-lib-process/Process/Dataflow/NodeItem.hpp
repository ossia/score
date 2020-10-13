#pragma once
#include <Process/ZoomHelper.hpp>

#include <score/model/Identifier.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_process_export.h>
namespace score
{
struct DocumentContext;
class SimpleTextItem;
class QGraphicsPixmapToggle;
}
namespace Dataflow
{
class PortItem;
}
namespace Process
{
class ProcessModel;
class LayerPresenter;
struct LayerContext;
struct Context;
}

namespace Process
{
class TitleItem;
class SCORE_LIB_PROCESS_EXPORT NodeItem
    : public QObject
    , public QGraphicsItem
{
public:
  NodeItem(const Process::ProcessModel& model, const Process::Context& ctx, QGraphicsItem* parent);
  const Id<Process::ProcessModel>& id() const noexcept;
  ~NodeItem();

  void setZoomRatio(ZoomRatio r);
  void setPlayPercentage(float f);

  qreal width() const noexcept;
  qreal height() const;

  const Process::ProcessModel& model() const noexcept;

private:
  void createContentItem();
  void updateSize();
  void setSize(QSizeF sz);

  bool isInSelectionCorner(QPointF f, QRectF r) const;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

  void resetInlets();
  void resetOutlets();

  QSizeF size() const noexcept;

  void setSelected(bool s);

  void updateTitlePos();
  QRectF boundingRect() const final override;

  static void paintNode(QPainter* painter, bool selected, bool hovered, QRectF rect);


  // Title
  QGraphicsItem* m_uiButton{};
  score::SimpleTextItem* m_label{};

  QSizeF m_contentSize{};

  const Process::ProcessModel& m_model;

  // Body
  QGraphicsItem* m_fx{};
  score::QGraphicsPixmapToggle* m_fold{};
  Process::LayerPresenter* m_presenter{};

  std::vector<Dataflow::PortItem*> m_inlets, m_outlets;
  const Process::Context& m_context;

  ZoomRatio m_ratio{1.};
  double m_playPercentage{};

  bool m_hover{false};
  bool m_selected{false};
};
}
