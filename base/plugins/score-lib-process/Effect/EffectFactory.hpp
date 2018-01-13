#pragma once
#include <Process/LayerModelPanelProxy.hpp>
#include <Process/LayerView.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/Process.hpp>
#include <QGraphicsSceneEvent>
#include <score/widgets/RectItem.hpp>

namespace Process
{
template <typename Model_T>
class EffectProcessFactory_T final : public Process::ProcessModelFactory
{
public:
  virtual ~EffectProcessFactory_T() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  { return Metadata<ConcreteKey_k, Model_T>::get(); }
  QString prettyName() const override
  { return Metadata<PrettyName_k, Model_T>::get(); }
  QString category() const override
  { return Metadata<Category_k, Model_T>::get(); }
  ProcessFlags flags() const override
  { return Metadata<ProcessFlags_k, Model_T>::get(); }

  QString customConstructionData() const override;

  Model_T* make(
      const TimeVal& duration,
      const QString& data,
      const Id<Process::ProcessModel>& id,
      QObject* parent) override
  {
    return new Model_T{duration, data, id, parent};
  }

  Model_T* load(const VisitorVariant& vis, QObject* parent) final override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new Model_T{deserializer, parent};
    });
  }
};

template <typename Model_T>
QString EffectProcessFactory_T<Model_T>::customConstructionData() const
{
  static_assert(std::is_same<Model_T, void>::value, "can't be used like this");
  return {};
}

class EffectLayerView final : public Process::LayerView
{
  public:
  EffectLayerView(QGraphicsItem* parent)
    : Process::LayerView{parent}
  {

  }
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
      emit pressed(ev->scenePos());
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

class EffectLayerPresenter final : public Process::LayerPresenter
{
public:
  EffectLayerPresenter(
      const Process::ProcessModel& model,
      EffectLayerView* view,
      const Process::ProcessPresenterContext& ctx,
      QObject* parent)
    : LayerPresenter{ctx, parent}, m_layer{model}, m_view{view}
  {
    putToFront();
    connect(view, &Process::LayerView::pressed, this, [&] {
      m_context.context.focusDispatcher.focus(this);
    });

    connect(
          m_view, &Process::LayerView::askContextMenu, this,
          &Process::LayerPresenter::contextMenuRequested);
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
  EffectLayerView* m_view{};
};

template<typename Model_T, typename Item_T, typename ExtView_T = void>
class EffectLayerFactory_T final : public Process::LayerFactory
{
public:
  virtual ~EffectLayerFactory_T() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  LayerView* makeLayerView(
      const Process::ProcessModel& viewmodel,
      QGraphicsItem* parent) final override
  {
    return new EffectLayerView{parent};
  }

  LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel& lm,
      Process::LayerView* v,
      const Process::ProcessPresenterContext& context,
      QObject* parent) final override
  {
    auto pres = new EffectLayerPresenter{
          safe_cast<const Model_T&>(lm),
          safe_cast<EffectLayerView*>(v), context, parent};

    auto rect = new score::RectItem{v};
    auto item = makeItem(lm, context, rect);
    item->setParentItem(rect);
    return pres;
  }

  QGraphicsItem* makeItem(
      const Process::ProcessModel& proc,
      const score::DocumentContext& ctx,
      score::RectItem* parent) const override
  {
    return new Item_T{safe_cast<const Model_T&>(proc), ctx, parent};
  }

  QWindow*
  makeExternalUI(const Process::ProcessModel& proc,
                 const score::DocumentContext& ctx,
                 QWidget* parent) override
  {
    if constexpr(!std::is_same_v<ExtView_T, void>)
      return new ExtView_T{safe_cast<const Model_T&>(proc), ctx, parent};
    else
      return nullptr;
  }

  LayerPanelProxy* makePanel(
      const Process::ProcessModel& viewmodel, const score::DocumentContext& ctx, QObject* parent) final override
  {
    return nullptr;
    //return new Process::GraphicsViewLayerPanelProxy{layer, parent};
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }
};

}
