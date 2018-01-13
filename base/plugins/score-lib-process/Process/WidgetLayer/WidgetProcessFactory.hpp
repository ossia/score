#pragma once
#include <Process/ProcessFactory.hpp>
#include <Process/WidgetLayer/WidgetLayerPanelProxy.hpp>
#include <Process/WidgetLayer/WidgetLayerPresenter.hpp>
#include <Process/WidgetLayer/WidgetLayerView.hpp>

namespace WidgetLayer
{
template <typename Model_T, typename Widget_T>
class LayerFactory final : public Process::LayerFactory
{
public:
  virtual ~LayerFactory() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }

  View* makeLayerView(
      const Process::ProcessModel&,
      QGraphicsItem* parent) final override
  {
    return new View{parent};
  }

  Presenter<Model_T, Widget_T>* makeLayerPresenter(
      const Process::ProcessModel& lm,
      Process::LayerView* v,
      const Process::ProcessPresenterContext& context,
      QObject* parent) final override
  {
    return new Presenter<Model_T, Widget_T>{safe_cast<const Model_T&>(lm),
                                            safe_cast<View*>(v), context,
                                            parent};
  }

  Process::LayerPanelProxy* makePanel(
      const Process::ProcessModel& viewmodel, const score::DocumentContext& ctx, QObject* parent) final override
  {
    return new LayerPanelProxy<Model_T, Widget_T>{viewmodel, parent};
  }
};

}
