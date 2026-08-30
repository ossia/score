#include "LayerFactory.hpp"

#include <ClipLauncher/Metadata.hpp>
#include <ClipLauncher/ProcessModel.hpp>
#include <ClipLauncher/View/ClipLauncherPresenter.hpp>
#include <ClipLauncher/View/ClipLauncherView.hpp>

#include <score/tools/SafeCast.hpp>

namespace ClipLauncher
{

LayerFactory::~LayerFactory() = default;

UuidKey<Process::ProcessModel> LayerFactory::concreteKey() const noexcept
{
  return Metadata<ConcreteKey_k, ProcessModel>::get();
}

bool LayerFactory::matches(const UuidKey<Process::ProcessModel>& p) const
{
  return p == Metadata<ConcreteKey_k, ProcessModel>::get();
}

Process::LayerPresenter* LayerFactory::makeLayerPresenter(
    const Process::ProcessModel& model, Process::LayerView* view,
    const Process::Context& context, QObject* parent) const
{
  return new ClipLauncherPresenter{
      safe_cast<const ProcessModel&>(model),
      safe_cast<ClipLauncherView*>(view), context, parent};
}

Process::LayerView* LayerFactory::makeLayerView(
    const Process::ProcessModel& proc, const Process::Context& context,
    QGraphicsItem* parent) const
{
  return new ClipLauncherView{parent};
}

} // namespace ClipLauncher
