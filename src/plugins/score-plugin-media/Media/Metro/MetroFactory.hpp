#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Media/Metro/MetroMetadata.hpp>
#include <Media/Metro/MetroModel.hpp>
#include <Media/Metro/MetroPresenter.hpp>
#include <Media/Metro/MetroView.hpp>

namespace Media
{
namespace Metro
{
using ProcessFactory = Process::ProcessFactory_T<Metro::Model>;
using LayerFactory
    = Process::LayerFactory_T<Metro::Model, Metro::Presenter, Metro::View>;
}
}
