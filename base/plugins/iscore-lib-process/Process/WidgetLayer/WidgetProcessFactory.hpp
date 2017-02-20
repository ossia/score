#pragma once
#include <Process/ProcessFactory.hpp>
#include <Process/WidgetLayer/WidgetLayerModel.hpp>
#include <Process/WidgetLayer/WidgetLayerPanelProxy.hpp>
#include <Process/WidgetLayer/WidgetLayerPresenter.hpp>
#include <Process/WidgetLayer/WidgetLayerView.hpp>

namespace WidgetLayer
{
template <typename Model_T, typename LayerModel_T, typename Widget_T>
class LayerFactory final : public Process::LayerFactory
{
public:
  virtual ~LayerFactory() = default;

private:
  UuidKey<Process::LayerFactory> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, LayerModel_T>::get();
  }

  bool matches(const UuidKey<Process::ProcessModelFactory>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }

  LayerModel_T* makeLayer_impl(
      Process::ProcessModel& proc,
      const Id<Process::LayerModel>& viewModelId,
      const QByteArray& constructionData,
      QObject* parent) final override;

  LayerModel_T* cloneLayer_impl(
      Process::ProcessModel& proc,
      const Id<Process::LayerModel>& newId,
      const Process::LayerModel& source,
      QObject* parent) final override;

  LayerModel_T* loadLayer_impl(
      Process::ProcessModel& proc,
      const VisitorVariant& vis,
      QObject* parent) final override;

  View* makeLayerView(
      const Process::LayerModel& viewmodel,
      QQuickPaintedItem* parent) final override
  {
    return new View{parent};
  }

  Presenter<Model_T, Widget_T>* makeLayerPresenter(
      const Process::LayerModel& lm,
      Process::LayerView* v,
      const Process::ProcessPresenterContext& context,
      QObject* parent) final override
  {
    return new Presenter<Model_T, Widget_T>{safe_cast<const LayerModel_T&>(lm),
                                            safe_cast<View*>(v), context,
                                            parent};
  }

  Process::LayerModelPanelProxy* makePanel(
      const Process::LayerModel& viewmodel, QObject* parent) final override
  {
    return new LayerPanelProxy<Model_T, Widget_T>{viewmodel, parent};
  }
};

template <typename Model_T, typename LayerModel_T, typename Widget_T>
LayerModel_T* LayerFactory<Model_T, LayerModel_T, Widget_T>::makeLayer_impl(
    Process::ProcessModel& proc,
    const Id<Process::LayerModel>& viewModelId,
    const QByteArray& constructionData,
    QObject* parent)
{
  return new LayerModel_T{static_cast<Model_T&>(proc), viewModelId, parent};
}

template <typename Model_T, typename LayerModel_T, typename Widget_T>
LayerModel_T* LayerFactory<Model_T, LayerModel_T, Widget_T>::cloneLayer_impl(
    Process::ProcessModel& proc,
    const Id<Process::LayerModel>& newId,
    const Process::LayerModel& source,
    QObject* parent)
{
  return new LayerModel_T{static_cast<const LayerModel_T&>(source),
                          static_cast<Model_T&>(proc), newId, parent};
}

template <typename Model_T, typename LayerModel_T, typename Widget_T>
LayerModel_T* LayerFactory<Model_T, LayerModel_T, Widget_T>::loadLayer_impl(
    Process::ProcessModel& proc, const VisitorVariant& vis, QObject* parent)
{
  return deserialize_dyn(vis, [&](auto&& deserializer) {
    auto layer
        = new LayerModel_T{deserializer, static_cast<Model_T&>(proc), parent};

    return layer;
  });
}
}
