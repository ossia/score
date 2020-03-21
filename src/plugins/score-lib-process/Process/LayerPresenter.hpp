#pragma once
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>

#include <score/model/Identifier.hpp>

#include <QGraphicsItem>

#include <score_lib_process_export.h>
#include <verdigris>
class QMenu;
class QPoint;
class QPointF;
namespace Process
{
class ProcessModel;
class LayerContextMenuManager;
class SCORE_LIB_PROCESS_EXPORT GraphicsShapeItem : public QGraphicsItem
{
public:
  using QGraphicsItem::QGraphicsItem;
  ~GraphicsShapeItem() override;
  virtual void setSize(QSizeF sz);
  virtual void on_zoomRatioChanged(ZoomRatio) = 0;

  QRectF boundingRect() const final override;

private:
  QSizeF m_sz{};
};
class SCORE_LIB_PROCESS_EXPORT LayerPresenter : public QObject
{
  W_OBJECT(LayerPresenter)

public:
  LayerPresenter(const ProcessModel& model, const LayerView* view, const Context& ctx, QObject* parent);
  ~LayerPresenter() override;

  const Process::LayerContext& context() const { return m_context; }

  bool focused() const;
  void setFocus(bool focus);
  virtual void on_focusChanged();

  virtual void setFullView();

  virtual void setWidth(qreal width, qreal defaultWidth) = 0;
  virtual void setHeight(qreal height) = 0;

  virtual void putToFront() = 0;
  virtual void putBehind() = 0;

  virtual void on_zoomRatioChanged(ZoomRatio) = 0;
  virtual void parentGeometryChanged() = 0;

  virtual const ProcessModel& model() const = 0;
  virtual const Id<ProcessModel>& modelId() const = 0;

  virtual void fillContextMenu(
      QMenu&,
      QPoint pos,
      QPointF scenepos,
      const LayerContextMenuManager&);

  virtual GraphicsShapeItem* makeSlotHeaderDelegate();

public:
  void contextMenuRequested(const QPoint& arg_1, const QPointF& arg_2)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, contextMenuRequested, arg_1, arg_2)

protected:
  Process::LayerContext m_context;

private:
  bool m_focus{false};
};
}

W_REGISTER_ARGTYPE(Process::LayerPresenter*)
