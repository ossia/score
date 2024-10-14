#pragma once
#include <Process/ProcessFactory.hpp>

#include <Effect/EffectLayer.hpp>

namespace oscr
{
template <typename T>
concept has_ossia_layer = requires { sizeof(typename T::Layer); };

template <has_ossia_layer T>
class ScoreLayerFactory final : public Process::LayerFactory
{
public:
  virtual ~ScoreLayerFactory() = default;

private:
  std::optional<double> recommendedHeight() const noexcept override
  {
    if constexpr(requires { T::recommended_height(); })
    {
      return T::recommended_height();
    }
    return LayerFactory::recommendedHeight();
  }

  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, oscr::ProcessModel<T>>::get();
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, oscr::ProcessModel<T>>::get();
  }

  Process::LayerView* makeLayerView(
      const Process::ProcessModel& proc, const Process::Context& context,
      QGraphicsItem* parent) const final override
  {
    return new typename T::Layer{proc, context, parent};
  }

  Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel& lm, Process::LayerView* v,
      const Process::Context& context, QObject* parent) const final override
  {
    auto view = static_cast<typename T::Layer*>(v);
    return new Process::EffectLayerPresenter{lm, view, context, parent};
  }

  score::ResizeableItem* makeItem(
      const Process::ProcessModel& proc, const Process::Context& ctx,
      QGraphicsItem* parent) const final override
  {
    return nullptr;
  }
};
}
