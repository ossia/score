#pragma once
#include <Curve/CurveStyle.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <score_plugin_curve_export.h>

namespace Curve
{
class EditionSettings;
template <
    typename Model_T,
    typename LayerPresenter_T,
    typename LayerView_T,
    typename CurveColors_T,
    typename HeaderDelegate_T>
class CurveLayerFactory_T final : public Process::LayerFactory, public StyleInterface
{
public:
  virtual ~CurveLayerFactory_T() = default;

  LayerView_T* makeLayerView(
      const Process::ProcessModel& viewmodel,
      const Process::Context& context,
      QGraphicsItem* parent) const final override
  {
    return new LayerView_T{parent};
  }

  LayerPresenter_T* makeLayerPresenter(
      const Process::ProcessModel& lm,
      Process::LayerView* v,
      const Process::Context& context,
      QObject* parent) const final override
  {
    return new LayerPresenter_T{
        m_colors.style(),
        safe_cast<const Model_T&>(lm),
        safe_cast<LayerView_T*>(v),
        context,
        parent};
  }

  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }

  const Curve::Style& style() const override { return m_colors.style(); }

  Process::HeaderDelegate* makeHeaderDelegate(
      const Process::ProcessModel& model,
      const Process::Context& ctx,
      QGraphicsItem* parent) const override
  {
    return new HeaderDelegate_T{model, ctx};
  }

private:
  CurveColors_T m_colors{score::Skin::instance()};
};
}
