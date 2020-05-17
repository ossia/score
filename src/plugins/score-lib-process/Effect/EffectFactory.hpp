#pragma once
#include <Process/LayerView.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>

#include <score/graphics/RectItem.hpp>

#include <Effect/EffectLayer.hpp>

namespace Process
{
template <typename Model_T>
class EffectProcessFactory_T final : public Process::ProcessModelFactory
{
public:
  virtual ~EffectProcessFactory_T() = default;

  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }
  QString prettyName() const override { return Metadata<PrettyName_k, Model_T>::get(); }
  QString category() const override { return Metadata<Category_k, Model_T>::get(); }

  Descriptor descriptor(QString) const override;

  ProcessFlags flags() const override { return Metadata<ProcessFlags_k, Model_T>::get(); }

  QString customConstructionData() const override;

  Model_T* make(
      const TimeVal& duration,
      const QString& data,
      const Id<Process::ProcessModel>& id,
      const score::DocumentContext& ctx,
      QObject* parent) override
  {
    return new Model_T{duration, data, id, parent};
  }

  Model_T* load(const VisitorVariant& vis, const score::DocumentContext& ctx, QObject* parent)
      final override
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

template <typename Model_T, typename Item_T, typename ExtView_T = void>
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
      const Process::ProcessModel&,
      const Process::Context& context,
      QGraphicsItem* parent) const final override
  {
    return new EffectLayerView{parent};
  }

  LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel& lm,
      Process::LayerView* v,
      const Process::Context& context,
      QObject* parent) const final override
  {
    auto pres = new EffectLayerPresenter{
        safe_cast<const Model_T&>(lm), safe_cast<EffectLayerView*>(v), context, parent};

    auto rect = new score::EmptyRectItem{v};
    auto item = makeItem(lm, context, rect);
    item->setParentItem(rect);
    return pres;
  }

  score::ResizeableItem* makeItem(
      const Process::ProcessModel& proc,
      const Process::Context& ctx,
      QGraphicsItem* parent) const override
  {
    return new Item_T{safe_cast<const Model_T&>(proc), ctx, parent};
  }

  bool hasExternalUI(const Process::ProcessModel& proc, const score::DocumentContext& ctx)
      const noexcept override
  {
    return ((Model_T&)proc).hasExternalUI();
  }

  QWidget* makeExternalUI(
      const Process::ProcessModel& proc,
      const score::DocumentContext& ctx,
      QWidget* parent) const final override
  {
    (void)parent;
    try
    {
      if constexpr (!std::is_same_v<ExtView_T, void>)
        return new ExtView_T{safe_cast<const Model_T&>(proc), ctx, parent};
    }
    catch (...)
    {
    }
    return nullptr;
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }
};
}
