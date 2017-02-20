#pragma once
#include <Process/ProcessFactory.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

namespace Process
{
struct default_t
{
};

template <typename Model_T>
class GenericProcessModelFactory final : public Process::ProcessModelFactory
{
public:
  virtual ~GenericProcessModelFactory() = default;

private:
  UuidKey<Process::ProcessModelFactory> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  QString prettyName() const override
  {
    return Metadata<PrettyName_k, Model_T>::get();
  }

  Model_T* make(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent) final override;

  Model_T* load(const VisitorVariant& vis, QObject* parent) final override;
};

template <typename Model_T>
Model_T* GenericProcessModelFactory<Model_T>::make(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
{
  return new Model_T{duration, id, parent};
}

template <typename Model_T>
Model_T* GenericProcessModelFactory<Model_T>::load(
    const VisitorVariant& vis, QObject* parent)
{
  return deserialize_dyn(vis, [&](auto&& deserializer) {
    return new Model_T{deserializer, parent};
  });
}

template <
    typename Model_T, typename LayerModel_T, typename LayerPresenter_T,
    typename LayerView_T, typename LayerPanel_T>
class GenericLayerFactory final : public Process::LayerFactory
{
public:
  virtual ~GenericLayerFactory() = default;

private:
  UuidKey<Process::LayerFactory> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, LayerModel_T>::get();
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

  LayerView_T* makeLayerView(
      const Process::LayerModel& viewmodel,
      QQuickPaintedItem* parent) final override
  {
    return new LayerView_T{parent};
  }

  LayerPresenter_T* makeLayerPresenter(
      const Process::LayerModel& lm,
      Process::LayerView* v,
      const Process::ProcessPresenterContext& context,
      QObject* parent) final override
  {
    return new LayerPresenter_T{safe_cast<const LayerModel_T&>(lm),
                                safe_cast<LayerView_T*>(v), context, parent};
  }

  LayerPanel_T* makePanel(
      const Process::LayerModel& viewmodel, QObject* parent) final override;

  bool matches(const UuidKey<Process::ProcessModelFactory>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }
};

template <
    typename Model_T, typename LayerModel_T, typename LayerPresenter_T,
    typename LayerView_T, typename LayerPanel_T>
LayerModel_T*
GenericLayerFactory<Model_T, LayerModel_T, LayerPresenter_T, LayerView_T, LayerPanel_T>::
    makeLayer_impl(
        Process::ProcessModel& proc,
        const Id<Process::LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
  return new LayerModel_T{static_cast<Model_T&>(proc), viewModelId, parent};
}

template <
    typename Model_T, typename LayerModel_T, typename LayerPresenter_T,
    typename LayerView_T, typename LayerPanel_T>
LayerModel_T*
GenericLayerFactory<Model_T, LayerModel_T, LayerPresenter_T, LayerView_T, LayerPanel_T>::
    cloneLayer_impl(
        Process::ProcessModel& proc,
        const Id<Process::LayerModel>& newId,
        const Process::LayerModel& source,
        QObject* parent)
{
  return new LayerModel_T{static_cast<const LayerModel_T&>(source),
                          static_cast<Model_T&>(proc), newId, parent};
}

template <
    typename Model_T, typename LayerModel_T, typename LayerPresenter_T,
    typename LayerView_T, typename LayerPanel_T>
LayerModel_T*
GenericLayerFactory<Model_T, LayerModel_T, LayerPresenter_T, LayerView_T, LayerPanel_T>::
    loadLayer_impl(
        Process::ProcessModel& proc,
        const VisitorVariant& vis,
        QObject* parent)
{
  return deserialize_dyn(vis, [&](auto&& deserializer) {
    auto layer
        = new LayerModel_T{deserializer, static_cast<Model_T&>(proc), parent};

    return layer;
  });
}

template <
    typename Model_T, typename LayerModel_T, typename LayerPresenter_T,
    typename LayerView_T, typename LayerPanel_T>
LayerPanel_T*
GenericLayerFactory<Model_T, LayerModel_T, LayerPresenter_T, LayerView_T, LayerPanel_T>::
    makePanel(const Process::LayerModel& viewmodel, QObject* parent)
{
  return new LayerPanel_T{static_cast<const LayerModel_T&>(viewmodel), parent};
}

// TODO we could just pass the Model_T since
// LayerModel_T has to be Process::LayerModel_T<Model_T> here

template <typename Model_T, typename LayerModel_T>
class
    GenericLayerFactory<Model_T, LayerModel_T, default_t, default_t, default_t>
    : // final :
      public Process::LayerFactory
{
public:
  virtual ~GenericLayerFactory() = default;

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
      QObject* parent) final override
  {
    return new LayerModel_T{static_cast<Model_T&>(proc), viewModelId, parent};
  }

  LayerModel_T* loadLayer_impl(
      Process::ProcessModel& p,
      const VisitorVariant& vis,
      QObject* parent) final override
  {
    return deserialize_dyn(vis, [&](auto&& deserializer) {
      auto autom = new LayerModel_T{deserializer, p, parent};

      return autom;
    });
  }

  LayerModel_T* cloneLayer_impl(
      Process::ProcessModel& p,
      const Id<Process::LayerModel>& newId,
      const Process::LayerModel& source,
      QObject* parent) final override
  {
    return new LayerModel_T{safe_cast<const LayerModel_T&>(source), p, newId,
                            parent};
  }
};

template <typename LayerModel_T>
using GenericDefaultLayerFactory = GenericLayerFactory<
    typename LayerModel_T::process_type, LayerModel_T, default_t, default_t,
    default_t>;
}
