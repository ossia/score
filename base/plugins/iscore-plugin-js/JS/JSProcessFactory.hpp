#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Process/ProcessFactory.hpp>
#include <JS/JSProcessModel.hpp>
#include <JS/JSStateProcess.hpp>
#include <JS/JSProcessMetadata.hpp>

#include <Process/StateProcessFactory.hpp>

namespace JS
{
using ProcessFactory = Process::GenericDefaultProcessFactory<JS::ProcessModel>;

using StateProcessFactory = Process::StateProcessFactory_T<JS::StateProcess>;
}
