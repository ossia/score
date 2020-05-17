#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Patternist/PatternModel.hpp>
#include <Patternist/PatternPresenter.hpp>
#include <Patternist/PatternView.hpp>

namespace Patternist
{
using ProcessFactory = Process::ProcessFactory_T<Patternist::ProcessModel>;
using LayerFactory
    = Process::LayerFactory_T<Patternist::ProcessModel, Patternist::Presenter, Patternist::View>;
}
