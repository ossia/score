#pragma once
#include <Process/ProcessMetadata.hpp>

#include <QString>

namespace Pd
{
class ProcessModel;
}

PROCESS_METADATA(
    , Pd::ProcessModel, "7b3b18ea-311b-40f9-b04e-60ec1fe05786", "PureData", "PureData",
    Process::ProcessCategory::Script, "Plugins", "Embed PureData in score",
    "ossia and the Pd team", {}, {}, {},
    Process::ProcessFlags::SupportsAll | Process::ProcessFlags::PutInNewSlot
        | Process::ProcessFlags::ControlSurface)
