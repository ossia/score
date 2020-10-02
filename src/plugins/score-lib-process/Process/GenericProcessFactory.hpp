#pragma once
#include <Process/ProcessFactory.hpp>

#include <score/serialization/VisitorCommon.hpp>

namespace Process
{
struct default_t
{
};

template <typename Model_T>
class ProcessFactory_T final : public Process::ProcessModelFactory
{
public:
  virtual ~ProcessFactory_T() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }
  QString prettyName() const override { return Metadata<PrettyName_k, Model_T>::get(); }
  QString category() const override { return Metadata<Category_k, Model_T>::get(); }
  Descriptor descriptor(QString) const override
  {
    return Metadata<Process::Descriptor_k, Model_T>::get();
  }
  ProcessFlags flags() const override { return Metadata<ProcessFlags_k, Model_T>::get(); }

  Model_T* make(
      const TimeVal& duration,
      const QString& data,
      const Id<Process::ProcessModel>& id,
      const score::DocumentContext& ctx,
      QObject* parent) final override;

  Model_T* load(const VisitorVariant& vis, const score::DocumentContext& ctx, QObject* parent)
      final override;
};

template <typename Model_T>
Model_T* ProcessFactory_T<Model_T>::make(
    const TimeVal& duration,
    const QString& data,
    const Id<Process::ProcessModel>& id,
    const score::DocumentContext& ctx,
    QObject* parent)
{
  if constexpr (
      std::is_constructible_v<Model_T, TimeVal, QString, Id<Process::ProcessModel>, QObject*>)
    return new Model_T{duration, data, id, parent};
  else if constexpr (std::is_constructible_v<
                         Model_T,
                         TimeVal,
                         Id<Process::ProcessModel>,
                         const score::DocumentContext&,
                         QObject*>)
    return new Model_T{duration, id, ctx, parent};
  else
    return new Model_T{duration, id, parent};
}

template <typename Model_T>
Model_T* ProcessFactory_T<Model_T>::load(
    const VisitorVariant& vis,
    const score::DocumentContext& ctx,
    QObject* parent)
{
  return score::deserialize_dyn(vis, [&](auto&& deserializer) {
    if constexpr (std::is_constructible_v<
                      Model_T,
                      decltype(deserializer),
                      const score::DocumentContext&,
                      QObject*>)
      return new Model_T{deserializer, ctx, parent};
    else
      return new Model_T{deserializer, parent};
  });
}

template <
    typename Model_T,
    typename LayerPresenter_T,
    typename LayerView_T,
    typename HeaderDelegate_T = default_t>
class LayerFactory_T final : public Process::LayerFactory
{
public:
  virtual ~LayerFactory_T() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  std::optional<double> recommendedHeight() const noexcept override
  {
    if constexpr(bool(LayerPresenter_T::recommendedHeight))
    {
      return LayerPresenter_T::recommendedHeight;
    }
    return LayerFactory::recommendedHeight();
  }

  LayerView_T* makeLayerView(
      const Process::ProcessModel& viewmodel,
      const Process::Context& context,
      QGraphicsItem* parent) const final override
  {
    if constexpr (std::is_constructible_v<
                      LayerView_T,
                      const Model_T&,
                      const Process::Context&,
                      QGraphicsItem*>)
      return new LayerView_T{safe_cast<const Model_T&>(viewmodel), context, parent};
    else if constexpr (std::is_constructible_v<LayerView_T, const Model_T&, QGraphicsItem*>)
      return new LayerView_T{safe_cast<const Model_T&>(viewmodel), parent};
    else
      return new LayerView_T{parent};
  }

  LayerPresenter_T* makeLayerPresenter(
      const Process::ProcessModel& lm,
      Process::LayerView* v,
      const Process::Context& context,
      QObject* parent) const final override
  {
    return new LayerPresenter_T{
        safe_cast<const Model_T&>(lm), safe_cast<LayerView_T*>(v), context, parent};
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }

  HeaderDelegate* makeHeaderDelegate(
      const ProcessModel& model,
      const Process::Context& ctx,
      QGraphicsItem* parent) const override
  {
    if constexpr (std::is_same_v<HeaderDelegate_T, default_t>)
      return LayerFactory::makeHeaderDelegate(model, ctx, parent);
    else
      return new HeaderDelegate_T{model, ctx};
  }
};

template <typename Model_T>
class LayerFactory_T<Model_T, default_t, default_t, default_t> : // final :
                                                                 public Process::LayerFactory
{
public:
  virtual ~LayerFactory_T() = default;

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
using GenericDefaultLayerFactory = LayerFactory_T<Model_T, default_t, default_t, default_t>;
}
