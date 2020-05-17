#pragma once
#include <Media/AudioChain/AudioChainLayer.hpp>
#include <Media/AudioChain/AudioChainModel.hpp>
#include <Process/GenericProcessFactory.hpp>
namespace Media::AudioChain
{
using ProcessFactory = Process::ProcessFactory_T<Media::AudioChain::ProcessModel>;

using LayerFactory
    = Process::LayerFactory_T<Media::AudioChain::ProcessModel, Media::Presenter, Media::View>;
}
