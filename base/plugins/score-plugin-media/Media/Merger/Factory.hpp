#pragma once
#include <Media/Merger/Metadata.hpp>
#include <Media/Merger/Model.hpp>
#include <Media/Merger/Presenter.hpp>
#include <Media/Merger/View.hpp>
#include <Process/GenericProcessFactory.hpp>

namespace Media
{
namespace Merger
{
using ProcessFactory = Process::ProcessFactory_T<Merger::Model>;
using LayerFactory = Process::GenericDefaultLayerFactory<Merger::Model>;
}
}
