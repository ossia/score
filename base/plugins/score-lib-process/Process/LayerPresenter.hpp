#pragma once
#include <Process/ZoomHelper.hpp>

#include <score_lib_process_export.h>

#include <Process/ProcessContext.hpp>
#include <score/model/Identifier.hpp>
#include <QGraphicsItem>
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

    QRectF boundingRect() const final override;
  private:
    QSizeF m_sz{};
};
class SCORE_LIB_PROCESS_EXPORT LayerPresenter : public QObject
{
  Q_OBJECT

public:
  LayerPresenter(const ProcessPresenterContext& ctx, QObject* parent)
      : QObject{parent}, m_context{ctx, *this}
  {
  }

  ~LayerPresenter() override;

  const Process::LayerContext& context() const
  {
    return m_context;
  }

  bool focused() const;
  void setFocus(bool focus);
  virtual void on_focusChanged();

  virtual void setFullView();

  virtual void setWidth(qreal width) = 0;
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

Q_SIGNALS:
  void contextMenuRequested(const QPoint&, const QPointF&);

protected:
  Process::LayerContext m_context;

private:
  bool m_focus{false};
};
}
