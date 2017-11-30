#pragma once
#include <Engine/Node/Process.hpp>
#include <Engine/Node/Widgets.hpp>
#include <Process/LayerView.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Dataflow/UI/PortItem.hpp>
#include <Scenario/Document/CommentBlock/TextItem.hpp>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

namespace Process
{

class ILayerView : public Process::LayerView
{
    Q_OBJECT
  public:
    using Process::LayerView::LayerView;

  signals:
    void pressed();
    void askContextMenu(const QPoint&, const QPointF&);
    void contextMenuRequested(QPoint);
};

struct RectItem : public QObject, public QGraphicsItem
{

    QRectF m_rect{};
public:
    using QGraphicsItem::QGraphicsItem;
    void setRect(QRectF r) { prepareGeometryChange(); m_rect = r; }
  QRectF boundingRect() const override
  {
    return m_rect;
  }
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(ScenarioStyle::instance().TransparentBrush);
    painter->setPen(ScenarioStyle::instance().MinimapPen);
    painter->drawRoundedRect(m_rect, 5, 5);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }

protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
  {
    this->setZValue(10);
  }
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
  {
    this->setZValue(0);
  }
};
template <typename Info>
class ControlLayerView final : public ILayerView
{
  public:
    explicit ControlLayerView(QGraphicsItem* parent)
      : ILayerView{parent}
    {
    }

    void setup(const ControlProcess<Info>& object,
               const score::DocumentContext& doc)
    {
      if constexpr(InfoFunctions<Info>::control_count > 0)
      {
        std::size_t i = 0;
        double pos_y = 0;
        ossia::for_each_in_tuple(
              get_controls(Info::info),
              [&] (const auto& ctrl) {
          auto item = new RectItem{this};
          item->setPos(0, pos_y);
          auto inlet = static_cast<ControlInlet*>(object.inlets_ref()[InfoFunctions<Info>::control_start + i]);

          auto port = Dataflow::setupInlet(*inlet, doc, item, this);


          auto lab = new Scenario::SimpleTextItem{item};
          lab->setColor(ScenarioStyle::instance().EventDefault);
          lab->setText(ctrl.name);

          QWidget* widg = ctrl.make_item(ctrl, *inlet, doc, nullptr, this);
          widg->setMaximumWidth(150);
          widg->setContentsMargins(0, 0, 0, 0);
          widg->setPalette(transparentPalette());
          widg->setAutoFillBackground(false);
          widg->setStyleSheet(transparentStylesheet());

          auto wrap = new QGraphicsProxyWidget{item};
          wrap->setWidget(widg);
          wrap->setContentsMargins(0, 0, 0, 0);

          lab->setPos(15, 2);
          wrap->setPos(15, lab->boundingRect().height());

          auto h = std::max(20., (qreal)(widg->height() + lab->boundingRect().height() + 2.));
          item->setRect(QRectF{0., 0., 170., h});
          port->setPos(7., h / 2.);

          pos_y += h;

          i++;
        });
      }
    }


  private:
    void paint_impl(QPainter*) const override
    {

    }
    void mousePressEvent(QGraphicsSceneMouseEvent* ev) override
    {
      if(ev && ev->button() == Qt::RightButton)
      {
        emit askContextMenu(ev->screenPos(), ev->scenePos());
      }
      else
      {
        emit pressed();
      }
      ev->accept();
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override
    {
      ev->accept();
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override
    {
      ev->accept();
    }

    void contextMenuEvent(QGraphicsSceneContextMenuEvent* ev) override
    {
      emit askContextMenu(ev->screenPos(), ev->scenePos());
      ev->accept();
    }

};

template <typename Info>
class ControlLayerPresenter final : public Process::LayerPresenter
{
public:
  explicit ControlLayerPresenter(
      const Process::ProcessModel& model,
      ControlLayerView<Info>* view,
      const Process::ProcessPresenterContext& ctx,
      QObject* parent)
      : LayerPresenter{ctx, parent}, m_layer{model}, m_view{view}
  {
    putToFront();
    connect(view, &ControlLayerView<Info>::pressed, this, [&]() {
      m_context.context.focusDispatcher.focus(this);
    });

    connect(
          m_view, &ControlLayerView<Info>::askContextMenu, this,
          &ControlLayerPresenter::contextMenuRequested);

    view->setup(static_cast<const ControlProcess<Info>&>(model), ctx);
  }

  void setWidth(qreal val) override
  {
    m_view->setWidth(val);
  }
  void setHeight(qreal val) override
  {
    m_view->setHeight(val);
  }

  void putToFront() override
  {
    m_view->setVisible(true);
  }

  void putBehind() override
  {
    m_view->setVisible(false);
  }

  void on_zoomRatioChanged(ZoomRatio) override
  {
  }

  void parentGeometryChanged() override
  {
  }

  const Process::ProcessModel& model() const override
  {
    return m_layer;
  }
  const Id<Process::ProcessModel>& modelId() const override
  {
    return m_layer.id();
  }

private:
  const Process::ProcessModel& m_layer;
  ControlLayerView<Info>* m_view{};
};

template <typename Info>
class ControlLayerFactory final : public Process::LayerFactory
{
public:
  virtual ~ControlLayerFactory() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, ControlProcess<Info>>::get();
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, ControlProcess<Info>>::get();
  }

  ControlLayerView<Info>* makeLayerView(
      const Process::ProcessModel& proc,
      QGraphicsItem* parent) final override
  {
    return new ControlLayerView<Info>{parent};
  }

  ControlLayerPresenter<Info>* makeLayerPresenter(
      const Process::ProcessModel& lm,
      Process::LayerView* v,
      const Process::ProcessPresenterContext& context,
      QObject* parent) final override
  {
    return new ControlLayerPresenter<Info>{safe_cast<const ControlProcess<Info>&>(lm),
                                            safe_cast<ControlLayerView<Info>*>(v), context,
                                            parent};
  }

  Process::LayerPanelProxy* makePanel(
      const Process::ProcessModel& viewmodel, QObject* parent) final override
  {
    return nullptr;
  }
};

}
