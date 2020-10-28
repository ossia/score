#pragma once
#include <Media/Step/Metadata.hpp>
#include <Media/Step/Model.hpp>
#include <Media/Step/Presenter.hpp>
#include <Media/Step/View.hpp>
#include <Process/GenericProcessFactory.hpp>

namespace Media::Step
{
using ProcessFactory = Process::ProcessFactory_T<Step::Model>;
struct LayerFactory : Process::LayerFactory_T<Step::Model, Step::Presenter, Step::View>
{
public:
  score::ResizeableItem* makeItem(const Process::ProcessModel&, const Process::Context& ctx, QGraphicsItem* parent) const override;
};
}
