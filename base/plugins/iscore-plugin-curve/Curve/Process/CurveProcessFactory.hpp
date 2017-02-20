#pragma once
#include <Curve/CurveStyle.hpp>
#include <Curve/Panel/CurvePanel.hpp>
#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore_plugin_curve_export.h>

namespace Curve
{
class EditionSettings;
template <
    typename Model_T, typename LayerModel_T, typename LayerPresenter_T,
    typename LayerView_T, typename CurveColors_T>
class CurveLayerFactory_T final : public Process::LayerFactory,
                                  public StyleInterface
{
public:
  virtual ~CurveLayerFactory_T() = default;

  Process::LayerModel* makeLayer_impl(
      Process::ProcessModel& proc,
      const Id<Process::LayerModel>& viewModelId,
      const QByteArray& constructionData,
      QObject* parent) final override
  {
    auto layer
        = new LayerModel_T{static_cast<Model_T&>(proc), viewModelId, parent};
    return layer;
  }

  Process::LayerModel* cloneLayer_impl(
      Process::ProcessModel& proc,
      const Id<Process::LayerModel>& newId,
      const Process::LayerModel& source,
      QObject* parent) final override
  {
    auto layer = new LayerModel_T{static_cast<const LayerModel_T&>(source),
                                  static_cast<Model_T&>(proc), newId, parent};
    return layer;
  }

  Process::LayerModel* loadLayer_impl(
      Process::ProcessModel& proc,
      const VisitorVariant& vis,
      QObject* parent) final override
  {
    return deserialize_dyn(vis, [&](auto&& deserializer) {
      auto layer = new LayerModel_T{deserializer, static_cast<Model_T&>(proc),
                                    parent};

      return layer;
    });
  }

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
    return new LayerPresenter_T{m_colors.style(),
                                safe_cast<const LayerModel_T&>(lm),
                                safe_cast<LayerView_T*>(v), context, parent};
  }

  Process::LayerModelPanelProxy*
  makePanel(const Process::LayerModel& layer, QObject* parent) override
  {
    return new CurvePanelProxy<LayerModel_T>{
        safe_cast<const LayerModel_T&>(layer), parent};
  }

  UuidKey<Process::LayerFactory> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, LayerModel_T>::get();
  }

  bool matches(const UuidKey<Process::ProcessModelFactory>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }

  const Curve::Style& style() const override
  {
    return m_colors.style();
  }

private:
  CurveColors_T m_colors{iscore::Skin::instance()};
};
}
