#pragma once
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>

#include <score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include <score/model/Identifier.hpp>
#include <score/widgets/MimeData.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_process_export.h>

#include <verdigris>
namespace score
{
struct DocumentContext;
class SimpleTextItem;
class ResizeableItem;
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
  W_OBJECT(NodeItem)
  Q_INTERFACES(QGraphicsItem)
public:
  NodeItem(
      const Process::ProcessModel& model, const Process::Context& ctx, TimeVal parentDur,
      QGraphicsItem* parent);
  const Id<Process::ProcessModel>& id() const noexcept;
  ~NodeItem();

  void setParentDuration(TimeVal r);
  void setPlayPercentage(float f, TimeVal parent_dur);

  qreal width() const noexcept;
  qreal height() const;

  const Process::ProcessModel& model() const noexcept;

  void dropReceived(const QPointF& pos, const QMimeData& arg_2)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, dropReceived, pos, arg_2)

  void resetDrop()
  {
    m_dropping = false;
    update();
  }

  static const constexpr int Type = QGraphicsItem::UserType + 5000;
  int type() const override { return Type; }

private:
  void createWithDecorations();
  void createContentItem();
  void createFoldedItem();
  void setupItem(score::ResizeableItem* resizeable);
  void updateTooltip();

  void createWithoutDecorations();

  void updateZoomRatio() const noexcept;
  void updateSize();
  void setSize(QSizeF sz);

  bool isInSelectionCorner(QPointF f, QRectF r) const;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event) override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;

  void resetItem();
  void resetInlets();
  void resetOutlets();
  void updateLabel();
  void resizeAsync();

  QSizeF size() const noexcept;

  void setSelected(bool s);

  void updateTitlePos();
  QRectF boundingRect() const final override;
  QRectF contentRect() const noexcept;
  void updateContentRect();

  double minimalContentWidth() const noexcept;
  double minimalContentHeight() const noexcept;
  static void paintNode(QPainter* painter, bool selected, bool hovered, QRectF rect);

  // Title
  QGraphicsItem* m_uiButton{};
  QGraphicsItem* m_presetButton{};
  score::SimpleTextItem* m_label{};

  QSizeF m_contentSize{};
  QRectF m_contentRect{};

  const Process::ProcessModel& m_model;

  // Body
  QGraphicsItem* m_fx{};
  score::QGraphicsPixmapToggle* m_fold{};
  Process::LayerPresenter* m_presenter{};

  std::vector<Dataflow::PortItem*> m_inlets, m_outlets;
  const Process::Context& m_context;
  MultiOngoingCommandDispatcher m_dispatcher;

  TimeVal m_parentDuration{1};
  double m_playPercentage{};

  bool m_hover : 1 {false};
  bool m_selected : 1 {false};
  bool m_dropping : 1 {};
  bool m_needResize : 1 {};
  bool m_folded : 1 {};
};
}
