#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Patternist/PatternModel.hpp>
#include <Patternist/PatternPresenter.hpp>
#include <Patternist/PatternView.hpp>

namespace Patternist
{
using ProcessFactory = Process::ProcessFactory_T<Patternist::ProcessModel>;
struct LayerFactory final
    : Process::LayerFactory_T<
          Patternist::ProcessModel, Patternist::Presenter, Patternist::View>
{
  bool hasExternalUI(const Process::ProcessModel&, const score::DocumentContext&)
      const noexcept override;
  QWidget* makeExternalUI(
      Process::ProcessModel&, const score::DocumentContext&,
      QWidget* parent) const override;
};
}
