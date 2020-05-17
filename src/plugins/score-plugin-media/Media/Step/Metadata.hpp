#pragma once
#include <Process/ProcessMetadata.hpp>

namespace Media
{
namespace Step
{
class Model;
}
}

PROCESS_METADATA(
    ,
    Media::Step::Model,
    "c953fa55-65f0-4b93-8bfc-54780250d2b8",
    "Step",
    "Step sequencer",
    Process::ProcessCategory::Generator,
    "Control",
    "Generates messages rhytmically on a given scale",
    "ossia score",
    {},
    {},
    {std::vector<Process::PortType>{Process::PortType::Message}},
    Process::ProcessFlags::SupportsTemporal | Process::ProcessFlags::PutInNewSlot)

UNDO_NAME_METADATA(EMPTY_MACRO, Media::Step::Model, "Step sequencer")
