#pragma once
#include <Process/ProcessFactory.hpp>

namespace ClipLauncher
{

class LayerFactory final : public Process::LayerFactory
{
public:
  ~LayerFactory() override;

  UuidKey<Process::ProcessModel> concreteKey() const noexcept override;
  bool matches(const UuidKey<Process::ProcessModel>& p) const override;

  Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel& model, Process::LayerView* view,
      const Process::Context& context, QObject* parent) const override;

  Process::LayerView*
  makeLayerView(const Process::ProcessModel& proc, const Process::Context& context, QGraphicsItem* parent)
      const override;
};

} // namespace ClipLauncher
