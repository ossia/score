#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Media/Merger/Metadata.hpp>
#include <Media/Merger/Model.hpp>

namespace Media
{
namespace Merger
{
using ProcessFactory = Process::ProcessFactory_T<Merger::Model>;
}
}
