#pragma once
#include <Media/AudioChain/AudioChainLayer.hpp>
#include <Media/SynthChain/SynthChainModel.hpp>
#include <Process/GenericProcessFactory.hpp>

namespace Media::SynthChain
{
using ProcessFactory = Process::ProcessFactory_T<SynthChain::ProcessModel>;

using LayerFactory
    = Process::LayerFactory_T<SynthChain::ProcessModel, Media::Presenter, Media::View>;
}
