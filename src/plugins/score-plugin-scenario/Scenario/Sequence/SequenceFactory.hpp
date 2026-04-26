#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Scenario/Sequence/SequenceModel.hpp>

namespace Sequence
{
using SequenceFactory = Process::ProcessFactory_T<Sequence::SequenceModel>;
}
