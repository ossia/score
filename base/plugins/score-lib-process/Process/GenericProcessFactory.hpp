#pragma once
#include <Process/ProcessFactory.hpp>
#include <score/serialization/VisitorCommon.hpp>

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
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  { return Metadata<ConcreteKey_k, Model_T>::get(); }
  QString prettyName() const override
  { return Metadata<PrettyName_k, Model_T>::get(); }
  QString category() const override
  { return Metadata<Category_k, Model_T>::get(); }

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
  return score::deserialize_dyn(vis, [&](auto&& deserializer) {
    return new Model_T{deserializer, parent};
  });
}

template <
    typename Model_T, typename LayerPresenter_T,
    typename LayerView_T, typename LayerPanel_T>
class GenericLayerFactory final : public Process::LayerFactory
{
public:
  virtual ~GenericLayerFactory() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  LayerView_T* makeLayerView(
      const Process::ProcessModel& viewmodel,
      QGraphicsItem* parent) final override
  {
    return new LayerView_T{parent};
  }

  LayerPresenter_T* makeLayerPresenter(
      const Process::ProcessModel& lm,
      Process::LayerView* v,
      const Process::ProcessPresenterContext& context,
      QObject* parent) final override
  {
    return new LayerPresenter_T{safe_cast<const Model_T&>(lm),
                                safe_cast<LayerView_T*>(v), context, parent};
  }

  LayerPanel_T* makePanel(
      const Process::ProcessModel& viewmodel, QObject* parent) final override;

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }
};

template <
    typename Model_T, typename LayerPresenter_T,
    typename LayerView_T, typename LayerPanel_T>
LayerPanel_T*
GenericLayerFactory<Model_T, LayerPresenter_T, LayerView_T, LayerPanel_T>::
    makePanel(const Process::ProcessModel& viewmodel, QObject* parent)
{
  return new LayerPanel_T{static_cast<const Model_T&>(viewmodel), parent};
}

template <typename Model_T>
class
    GenericLayerFactory<Model_T, default_t, default_t, default_t>
    : // final :
      public Process::LayerFactory
{
public:
  virtual ~GenericLayerFactory() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }
};

template <typename Model_T>
using GenericDefaultLayerFactory = GenericLayerFactory<
    Model_T, default_t, default_t, default_t>;
}
