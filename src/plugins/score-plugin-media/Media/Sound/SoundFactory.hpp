#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Media/Sound/SoundMetadata.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Media/Sound/SoundPresenter.hpp>
#include <Media/Sound/SoundView.hpp>

namespace Media
{
namespace Sound
{
using ProcessFactory = Process::ProcessFactory_T<Sound::ProcessModel>;
using LayerFactory = Process::LayerFactory_T<
    Sound::ProcessModel, Sound::LayerPresenter, Sound::LayerView>;
}
}
