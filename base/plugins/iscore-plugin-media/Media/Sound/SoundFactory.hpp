#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Media/Sound/SoundLayer.hpp>
#include <Media/Sound/SoundMetadata.hpp>
#include <Media/Sound/SoundPresenter.hpp>
#include <Media/Sound/SoundView.hpp>
#include <Process/LayerModelPanelProxy.hpp>

namespace Media
{
namespace Sound
{
using ProcessFactory = Process::GenericProcessModelFactory<Sound::ProcessModel>;
using LayerFactory = Process::GenericLayerFactory<
    Sound::ProcessModel,
    Sound::LayerPresenter,
    Sound::LayerView,
    Process::GraphicsViewLayerPanelProxy>;
}
}
