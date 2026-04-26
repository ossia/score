#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Scenario/Sequence/SequenceModel.hpp>
#include <Scenario/Sequence/SequencePresenter.hpp>
#include <Scenario/Sequence/SequenceView.hpp>

namespace Sequence
{
using SequenceLayerFactory = Process::
    LayerFactory_T<SequenceModel, SequencePresenter, SequenceView>;
} // namespace Sequence
